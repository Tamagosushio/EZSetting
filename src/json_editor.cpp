#include "json_editor.hpp"


void HistoryManager::Push(const EditAction& action) {
  undo_stack_.push(action);
  while (!redo_stack_.empty()) {
    redo_stack_.pop();
  }
}

bool HistoryManager::CanUndo() const {
  return !undo_stack_.empty();
}

bool HistoryManager::CanRedo() const {
  return !redo_stack_.empty();
}

const EditAction* HistoryManager::Undo() {
  if (!CanUndo()) return nullptr;
  EditAction action = undo_stack_.top();
  undo_stack_.pop();
  action.undo();
  redo_stack_.push(action);
  return &redo_stack_.top();
}

const EditAction* HistoryManager::Redo() {
  if (!CanRedo()) return nullptr;
  EditAction action = redo_stack_.top();
  redo_stack_.pop();
  action.redo();
  undo_stack_.push(action);
  return &undo_stack_.top();
}

JsonEditor::JsonEditor(json& data, const std::string& filename, std::function<void()> on_quit)
  : input_json_(data), filename_(filename), on_quit_(on_quit), selected_tree_item_index_(0), selected_editor_tab_index_(0), search_from_root_(true) {
  // メインUIコンポーネント
  edit_component_ = Input(&editable_content_, "Enter value (e.g., \"text\", 123, true, null)", edit_input_option_);
  edit_component_ |= CatchEvent([this](Event event) {
    if (event == Event::Return) {
      OnEditorEnter();
      tree_menu_->TakeFocus();
      return true;
    }
    return false;
  });
  menu_option_.on_change = [this] { UpdateEditorPane(); };
  menu_option_.on_enter = [this] { OnTreeEnter(); };
  menu_option_.entries_option.transform = [this](const EntryState& state) {
    Element element = text(state.label);
    if (0 <= state.index && state.index < entries_.size()) {
        element |= GetColorFromType(entries_[state.index].type);
    }
    if (state.active && tree_menu_->Focused()) {
      element |= inverted;
    }
    return element;
  };
  tree_menu_ = Menu(&menu_entries_, &selected_tree_item_index_, menu_option_);
  breadcrumb_component_ = std::make_shared<BreadcrumbComponent>(
    std::vector<std::string>{"root"},
    [this](int index) {
      if (index == 0) current_path_.clear();
      else current_path_.erase(current_path_.begin() + index, current_path_.end());
      UpdateBreadcrumbComponent();
      UpdateTreeEntries();
      selected_tree_item_index_ = 0;
      UpdateEditorPane();
    }
  );
  // メインレイアウトとモーダルコンポーネント
  main_layout_ = BuildMainLayout();
  add_modal_ = BuildAddModal();
  delete_modal_ = BuildDeleteModal();
  rename_modal_ = BuildRenameModal();
  search_modal_ = BuildSearchModal();
  // 全コンポーネントの管理
  modal_container_ = Container::Tab({
    main_layout_,
    add_modal_,
    delete_modal_,
    rename_modal_,
    search_modal_,
  }, &modal_state_);
  // 状態初期化
  UpdateTreeEntries();
  UpdateEditorPane();
  UpdateBreadcrumbComponent();
  tree_menu_->TakeFocus();
}

Component JsonEditor::GetLayout() {
  return Renderer(modal_container_, [this] {
    Element document = main_layout_->Render();
    if (modal_state_ == 1) {
      document = dbox({
        document,
        add_modal_->Render() | clear_under | center,
      });
    } else if (modal_state_ == 2) {
      document = dbox({
        document,
        delete_modal_->Render() | clear_under | center,
      });
    } else if (modal_state_ == 3) {
      document = dbox({
        document,
        rename_modal_->Render() | clear_under | center,
      });
    } else if (modal_state_ == 4) {
      document = dbox({
        document,
        search_modal_->Render() | clear_under | center,
      });
    }
    return document;
  });
}

Component JsonEditor::BuildMainLayout() {
  // 左ペイン
  auto tree_pane = Renderer(tree_menu_, [this] {
    return tree_menu_->Render() | border;
  });
  // 右ペイン
  auto editor_component = Container::Tab({
    Renderer([this] { return RenderViewer(); }),
    edit_component_,
  }, &selected_editor_tab_index_);
  auto editor_pane = Renderer(editor_component, [this, editor_component] {
    auto title = "View/Edit: " + GetCurrentSelectionKey();
    return vbox({
      text(title) | bold,
      separator(),
      editor_component->Render() | flex,
    }) | yframe | border;
  });
  // 上部
  auto breadcrumbs_bar = Renderer(breadcrumb_component_, [this] {
    return breadcrumb_component_->Render() | center | borderLight;
  });
  // 下部
  auto status_bar = Renderer([this] {
    return hbox({
      text("File: " + filename_),
      filler(),
      text(editor_hint_) | dim,
      filler(),
      text("[a] Add (Key/Value) | [d] Delete | [r] Rename | [z] Undo | [y] Redo | [q] Quit") | dim,
    }) | borderLight;
  });
  // 全体レイアウト
  auto main_container = Container::Horizontal({
    tree_pane | size(WIDTH, GREATER_THAN, 32),
    editor_pane | flex,
  });
  auto main_layout = Container::Vertical({
    breadcrumbs_bar,
    main_container | flex,
    status_bar,
  });
  main_layout |= CatchEvent([this](Event event) {
    if (modal_state_ == 0) {
      if (edit_component_->Focused()) {
        return false;
      }
      if (event == Event::Character('a')) {
        return OnOpenAddModal();
      }
      if (event == Event::Character('d')) {
        return OnOpenDeleteModal();
      }
      if (event == Event::Character('r')) {
        return OnOpenRenameModal();
      }
      if (event == Event::Character('q')) {
        if (on_quit_) on_quit_();
        return true;
      }
      if (event == Event::Character('z')) {
        PerformUndo();
        return true;
      }
      if (event == Event::Character('y')) {
        PerformRedo();
        return true;
      }
      if (event == Event::Character('/')) {
        return OnOpenSearchModal();
      }
    }
    return false;
  });
  return main_layout;
}

Element JsonEditor::RenderViewer() {
  return paragraph(viewer_content_);
}

void JsonEditor::UpdateBreadcrumbComponent() {
  std::vector<std::string> entries{"root"};
  entries.insert(entries.end(), current_path_.begin(), current_path_.end());
  breadcrumb_component_->SetEntries(entries);
}

void JsonEditor::UpdateTreeEntries() {
  entries_.clear();
  menu_entries_.clear();
  if (!current_path_.empty()) {
    entries_.push_back({"..", "..", json::value_t::discarded});
    menu_entries_.push_back("..");
  }
  json& node = GetNode(input_json_, current_path_);
  if (node.is_object()) {
    for (auto& [key, val] : node.items()) {
      std::string label = key;
      if (val.is_object()) label += " (Object)";
      else if (val.is_array()) label += " (Array)";
      entries_.push_back({label, key, val.type()});
      menu_entries_.push_back(label);
    }
  } else if (node.is_array()) {
    for (size_t i = 0; i < node.size(); ++i) {
      std::string key = std::to_string(i);
      json& val = node[i];
      std::string label = key;
      if (val.is_object()) label += " (Object)";
      else if (val.is_array()) label += " (Array)";
      entries_.push_back({label, key, val.type()});
      menu_entries_.push_back(label);
    }
  }
}

void JsonEditor::OnTreeEnter() {
  if (entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= entries_.size()) return;
  const auto& entry = entries_[selected_tree_item_index_];
  bool path_changed = false;
  if (entry.key == "..") {
    if (!current_path_.empty()) {
      current_path_.pop_back();
      path_changed = true;
    }
  } else {
    json& node = GetNode(input_json_, current_path_);
    json& selected_node = (node.is_array()) ? node[std::stoul(entry.key)] : node[entry.key];
    if (selected_node.is_object() || selected_node.is_array()) {
      current_path_.push_back(entry.key);
      path_changed = true;
    } else {
      edit_component_->TakeFocus();
    }
  }
  if (path_changed) {
    UpdateBreadcrumbComponent();
    UpdateTreeEntries();
    selected_tree_item_index_ = 0;
    UpdateEditorPane();
  }
}

void JsonEditor::UpdateEditorPane() {
  editor_hint_ = "";
  std::string key;
  json* selected_node = GetCurrentSelectedNode(key);
  if (!selected_node) {
    selected_editor_tab_index_ = 0;
    viewer_content_ = (menu_entries_.empty() || GetCurrentSelectionKey() == "[None]")
      ? "Select an item from the left."
      : "Select an item to view/edit.";
    return;
  }
  if (selected_node->is_primitive() || selected_node->is_null()) {
    selected_editor_tab_index_ = 1;
    if (selected_node->is_null()) {
      editable_content_ = "null";
    } else {
      editable_content_ = selected_node->dump();
    }
    editor_hint_ = "[Enter] to save change";
  } else {
    selected_editor_tab_index_ = 0;
    try {
      viewer_content_ = selected_node->dump(2);
    } catch (json::exception& error) {
      viewer_content_ = "Error reading JSON value: " + std::string(error.what());
    }
  }
}

void JsonEditor::OnEditorEnter() {
  std::string key;
  json* node_ptr = GetCurrentSelectedNode(key);
  if (!node_ptr) {
    tree_menu_->TakeFocus();
    return;
  }
  json old_value = *node_ptr;
  json& parent_node = GetNode(input_json_, current_path_);
  UpdateJsonValue(parent_node, key, editable_content_);
  json* new_node_ptr = nullptr;
  if (node_ptr->is_array()) {
    try {
      new_node_ptr = &parent_node[std::stoul(key)];
    } catch (...) { }
  } else {
    new_node_ptr = &parent_node[key];
  }
  if (new_node_ptr && old_value != *new_node_ptr) {
    json new_value = *new_node_ptr;
    std::vector<std::string> path = current_path_;
    history_manager_.Push({
      [this, path, key, old_value]() { ExecuteEditValue(path, key, old_value); },
      [this, path, key, new_value]() { ExecuteEditValue(path, key, new_value); },
      path,
      key,
    });
  }
  UpdateTreeEntries();
  tree_menu_->TakeFocus();
}

void JsonEditor::UpdateJsonValue(json& parent_node, const std::string& key, const std::string& new_value) {
  std::string cleaned_value = CleanStringForJson(new_value);
  json* target_node = nullptr;
  if (parent_node.is_array()) {
    try {
      target_node = &parent_node[std::stoul(key)];
    } catch (...) {
      return;
    }
  } else if (parent_node.is_object()) {
    target_node = &parent_node[key];
  } else {
    return;
  }
  try {
    json parsed_value = json::parse(cleaned_value);
    *target_node = parsed_value;
  } catch (...) {
    *target_node = cleaned_value;
  }
}

Component JsonEditor::BuildAddModal() {
  add_key_input_ = Input(&new_key_, "New Key", InputOption{.on_enter = [this]{ OnAddSubmit(); }});
  add_key_input_ |= CatchEvent([this](Event event) {
    if (event == Event::Return) {
      OnAddSubmit();
      return true;
    }
    return false;
  });
  add_value_input_ = Input(&new_value_, "Value (JSON literal)", InputOption{.on_enter = [this]{ OnAddSubmit(); }});
  add_value_input_ |= CatchEvent([this](Event event) {
    if (event == Event::Return) {
      OnAddSubmit();
      return true;
    }
    return false;
  });
  auto buttons = Container::Horizontal({
    Button("OK", [this] { OnAddSubmit(); }, GetModalButtonOption()) | size(WIDTH, EQUAL, 12),
    Button("Cancel", [this] { modal_state_ = 0; tree_menu_->TakeFocus(); }, GetModalButtonOption()) | size(WIDTH, EQUAL, 12),
  });
  auto modal = Container::Vertical({
    add_key_input_,
    add_value_input_,
    buttons,
  });
  auto modal_renderer = Renderer(modal, [this, buttons] {
    json& node = GetNode(input_json_, current_path_);
    Element input_field = nullptr;
    std::string title = "Add Entry";
    if (node.is_object()) {
      title = "Add New Key (Value will be null)";
      input_field = add_key_input_->Render();
    } else if (node.is_array()) {
      title = "Add New Value to Array";
      input_field = add_value_input_->Render();
    }
    return vbox({
      text(title) | center,
      separator(),
      input_field,
      buttons->Render() | center,
    }) | border;
  });
  return ApplyModalBehavors(modal_renderer);
}

bool JsonEditor::OnOpenAddModal() {
  json& node = GetNode(input_json_, current_path_);
  if (node.is_object()) {
    new_key_ = "";
    modal_state_ = 1;
    add_key_input_->TakeFocus();
    return true;
  } else if (node.is_array()) {
    new_value_ = "null";
    modal_state_ = 1;
    add_value_input_->TakeFocus();
    return true;
  }
  editor_hint_ = "Error: Can only add to Objects or Arrays.";
  return false;
}

void JsonEditor::OnAddSubmit() {
  json& node = GetNode(input_json_, current_path_);
  int new_index = -1;
  std::vector<std::string> path = current_path_;
  if (node.is_object()) {
    std::string cleaned_key = CleanStringForJson(new_key_);
    if (cleaned_key.empty()) {
      editor_hint_ = "Error: Key cannot be empty.";
      add_key_input_->TakeFocus();
      return;
    }
    ExecuteAddKey(current_path_, cleaned_key, nullptr);
    history_manager_.Push({
      [this, path, cleaned_key]() { ExecuteRemoveKey(path, cleaned_key); },
      [this, path, cleaned_key]() { ExecuteAddKey(path, cleaned_key, nullptr); },
      path,
      cleaned_key,
    });
    UpdateTreeEntries();
    new_index = GetIndexFromEntries(cleaned_key);
  } else if (node.is_array()) {
    std::string cleaned_value = CleanStringForJson(new_value_);
    json parsed_value;
    try {
      parsed_value = json::parse(cleaned_value);
    } catch (...) {
      parsed_value = cleaned_value;
    }
    ExecuteAddArrayElement(current_path_, parsed_value);
    history_manager_.Push({
      [this, path]() { ExecuteRemoveLastArrayElement(path); },
      [this, path, parsed_value]() { ExecuteAddArrayElement(path, parsed_value); },
      path,
      std::to_string(node.size() - 1),
    });
    UpdateTreeEntries();
    new_index = static_cast<int>(entries_.size() - 1);
  } else {
    modal_state_ = 0;
    tree_menu_->TakeFocus();
    return;
  }
  RefreshTreeAndCloseModal(new_index);
}

Component JsonEditor::BuildDeleteModal() {
  auto buttons = Container::Horizontal({
    Button("Yes (Delete)", [this] { OnDeleteSubmit(); }, GetModalButtonOption()) | size(WIDTH, EQUAL, 18),
    Button("No (Cancel)", [this] { modal_state_ = 0; tree_menu_->TakeFocus(); }, GetModalButtonOption()) | size(WIDTH, EQUAL, 18),
  });
  auto modal_renderer = Renderer(buttons, [this, buttons] {
    return vbox({
      text("Are you sure you want to delete this item?") | center,
      text("This action cannnot be undone.") | center,
      separator(),
      text("Item: " + GetCurrentSelectionKey()) | center,
      separator(),
      buttons->Render() | center,
    }) | border;
  });
  return ApplyModalBehavors(modal_renderer);
}

bool JsonEditor::OnOpenDeleteModal() {
  std::string key = GetCurrentSelectionKey();
  if (key == "[None]" || key == "..") {
    editor_hint_ = "Error: Cannot delete this item.";
    return false;
  }
  modal_state_ = 2;
  return true;
}

void JsonEditor::OnDeleteSubmit() {
  std::string key = GetCurrentSelectionKey();
  if (key == "[None]" || key == "..") return;
  json& node = GetNode(input_json_, current_path_);
  std::vector<std::string> path = current_path_;
  json deleted_value;
  int deleted_index = -1;
  try {
    if (node.is_object()) {
      deleted_value = node[key];
      ExecuteRemoveKey(current_path_, key);
      history_manager_.Push({
        [this, path, key, deleted_value]() { ExecuteAddKey(path, key, deleted_value); },
        [this, path, key]() { ExecuteRemoveKey(path, key); },
        path,
        key,
      });
    } else if (node.is_array()) {
      deleted_index = std::stoul(key);
      deleted_value = node[deleted_index];
      ExecuteRemoveArrayElement(current_path_, deleted_index);
      history_manager_.Push({
        [this, path, deleted_index, deleted_value]() { ExecuteInsertArrayElement(path, deleted_index, deleted_value); },
        [this, path, deleted_index]() { ExecuteRemoveArrayElement(path, deleted_index); },
        path,
        std::to_string(deleted_index > 0 ? deleted_index - 1 : 0),
      });
    }
  } catch (...) {
    editor_hint_ = "Error: Failed to delete item.";
  }
  RefreshTreeAndCloseModal(selected_tree_item_index_ - 1);
}

Component JsonEditor::BuildRenameModal() {
  rename_key_input_ = Input(&rename_key_, "Rename Key", InputOption{.on_enter = [this]{ OnRenameSubmit(); }});
  rename_key_input_ |= CatchEvent([this](Event event) {
    if (event == Event::Return) {
      OnRenameSubmit();
      return true;
    }
    return false;
  });
  auto buttons = Container::Horizontal({
    Button("OK", [this] { OnAddSubmit(); }, GetModalButtonOption()) | size(WIDTH, EQUAL, 12),
    Button("Cancel", [this] { modal_state_ = 0; tree_menu_->TakeFocus(); }, GetModalButtonOption()) | size(WIDTH, EQUAL, 12),
  });
  auto modal = Container::Vertical({
    rename_key_input_,
    buttons,
  });
  auto modal_renderer = Renderer(modal, [this, buttons] {
    json& node = GetNode(input_json_, current_path_);
    if (node.is_array()) {
      return vbox({
        text("Cannot Rename an Element in an Array"),
        separator(),
        Button("Go Back", [this] { modal_state_ = 0; tree_menu_->TakeFocus(); })->Render(),
      }) | border;
    }
    return vbox({
      text("Rename This Key") | center,
      separator(),
      rename_key_input_->Render(),
      buttons->Render() | center,
    }) | border;
  });
  return ApplyModalBehavors(modal_renderer);
}

bool JsonEditor::OnOpenRenameModal() {
  std::string key = GetCurrentSelectionKey();
  if (key == "[None]" || key == ".." || GetNode(input_json_, current_path_).is_array()) {
    editor_hint_ = "Error: Cannot rename this item.";
    return false;
  }
  rename_key_ = key;
  modal_state_ = 3;
  rename_key_input_->TakeFocus();
  return true;
}

void JsonEditor::OnRenameSubmit() {
  json& node = GetNode(input_json_, current_path_);
  if (!node.is_object()) {
    modal_state_ = 0;
    tree_menu_->TakeFocus();
    return;
  }
  std::string cleaned_key = CleanStringForJson(rename_key_);
  if (cleaned_key.empty()) {
    editor_hint_ = "Error: Key cannot be empty.";
    rename_key_input_->TakeFocus();
    return;
  }
  std::string current_key = GetCurrentSelectionKey();
  if (cleaned_key != current_key && node.contains(cleaned_key)) {
    editor_hint_ = "Error: This key is already in use.";
    rename_key_input_->TakeFocus();
    return;
  }
  ExecuteRenameKey(current_path_, current_key, cleaned_key);
  std::vector<std::string> path = current_path_;
  history_manager_.Push({
    [this, path, current_key, cleaned_key]() { ExecuteRenameKey(path, cleaned_key, current_key); },
    [this, path, current_key, cleaned_key]() { ExecuteRenameKey(path, current_key, cleaned_key); },
    path,
    cleaned_key,
  });
  UpdateTreeEntries();
  int new_index = GetIndexFromEntries(cleaned_key);
  RefreshTreeAndCloseModal(new_index);
}

Component JsonEditor::BuildSearchModal() {
  search_input_ = Input(&search_query_, "Search", InputOption{.on_enter = [this]{ OnSearchSubmit(); }});
  search_input_ |= CatchEvent([this](Event event) {
    if (event == Event::Return) {
      search_results_menu_->TakeFocus();
      OnSearchSubmit();
      return true;
    }
    return false;
  });
  search_from_root_checkbox_ = Checkbox("Search from root", &search_from_root_);
  search_menu_option_.on_enter = [this] { OnSearchResultEnter(); };
  search_results_menu_ = Menu(&search_result_labels_, &current_search_result_index_, search_menu_option_);
  auto modal_layout = Container::Vertical({
    search_input_,
    search_from_root_checkbox_,
    search_results_menu_,
  });
  auto modal_renderer = Renderer(modal_layout, [this] {
    return vbox({
      text("Search") | center,
      separator(),
      search_input_->Render(),
      search_from_root_checkbox_->Render() | center,
      separator(),
      (search_result_labels_.empty()) ? text("No results") | center : search_results_menu_->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 10),
    }) | border | size(WIDTH, GREATER_THAN, 40);
  });
  return ApplyModalBehavors(modal_renderer);
}

bool JsonEditor::OnOpenSearchModal() {
  search_query_ = "";
  search_result_labels_.clear();
  search_results_.clear();
  modal_state_ = 4;
  search_input_->TakeFocus();
  return true;
}

void JsonEditor::OnSearchSubmit() {
  if (search_query_.empty()) {
    return;
  }
  search_results_.clear();
  search_result_labels_.clear();
  current_search_result_index_ = 0;
  // 再帰検索ロジック
  std::function<void(const json&, std::vector<std::string>)> search_recursive = [&](const json& node, std::vector<std::string> path) {
    auto get_path_string = [](const std::vector<std::string>& p) {
      std::string s = "";
      for (size_t i = 0; i < p.size(); ++i) {
        s += p[i];
        if (i < p.size() - 1) s += " > ";
      }
      return s;
    };
    if (node.is_object()) {
      for (auto& [key, val] : node.items()) {
        std::vector<std::string> current_path = path;
        current_path.push_back(key);
        // キーの部分一致
        if (key.find(search_query_) != std::string::npos) {
          search_results_.push_back(current_path);
          search_result_labels_.push_back("Key: " + key + " (Path: " + get_path_string(current_path) + ")");
        }
        // 値(文字列)の部分一致
        if (val.is_string() && val.get<std::string>().find(search_query_) != std::string::npos) {
          search_results_.push_back(current_path);
          search_result_labels_.push_back("Val: " + val.get<std::string>() + " (Path: " + get_path_string(current_path) + ")");
        }
        search_recursive(val, current_path);
      }
    } else if (node.is_array()) {
      for (size_t i = 0; i < node.size(); ++i) {
        std::vector<std::string> current_path = path;
        current_path.push_back(std::to_string(i));
        // 値(文字列)の部分一致
        if (node[i].is_string() && node[i].get<std::string>().find(search_query_) != std::string::npos) {
          search_results_.push_back(current_path);
          search_result_labels_.push_back("Val: " + node[i].get<std::string>() + " (Path: " + get_path_string(current_path) + ")");
        }
        search_recursive(node[i], current_path);
      }
    }
  };

  if (search_from_root_) {
    search_recursive(input_json_, {});
  } else {
    json& current_node = GetNode(input_json_, current_path_);
    search_recursive(current_node, current_path_);
  }
  if (search_results_.empty()) {
    search_result_labels_.push_back("No results found.");
    search_input_->TakeFocus();
  } else {
    search_results_menu_->TakeFocus();
  }
}

void JsonEditor::OnSearchResultEnter() {
  if (search_results_.empty() || current_search_result_index_ < 0 || current_search_result_index_ >= search_results_.size()) {
    return;
  }
  std::vector<std::string> target_path = search_results_[current_search_result_index_];
  std::string target_key = target_path.back();
  target_path.pop_back();
  current_path_ = target_path;
  UpdateBreadcrumbComponent();
  UpdateTreeEntries();
  int index = GetIndexFromEntries(target_key);
  RefreshTreeAndCloseModal(index);
}

Component JsonEditor::ApplyModalBehavors(Component modal) {
  modal |= CatchEvent([this](Event event) {
    if (event == Event::Escape) {
      modal_state_ = 0;
      tree_menu_->TakeFocus();
      return true;
    }
    return false;
  });
  return modal;
}

void JsonEditor::RefreshTreeAndCloseModal(int focus_index) {
  UpdateTreeEntries();
  if (focus_index < 0 || focus_index >= entries_.size()) {
    focus_index = 0;
  }
  selected_tree_item_index_ = focus_index;
  UpdateEditorPane();
  tree_menu_->TakeFocus();
  modal_state_ = 0;
}

void JsonEditor::PerformUndo() {
  if (history_manager_.CanUndo()) {
    const EditAction* action = history_manager_.Undo();
    if (action) {
      RestoreView(*action);
    }
  }
}

void JsonEditor::PerformRedo() {
  if (history_manager_.CanRedo()) {
    const EditAction* action = history_manager_.Redo();
    if (action) {
      RestoreView(*action);
    }
  }
}

void JsonEditor::RestoreView(const EditAction& action) {
  current_path_ = action.path;
  UpdateBreadcrumbComponent();
  UpdateTreeEntries();
  const int new_index = GetIndexFromEntries(action.focus_key);
  selected_tree_item_index_ = new_index;
  UpdateEditorPane();
  tree_menu_->TakeFocus();
}

void JsonEditor::ExecuteEditValue(const std::vector<std::string>& path, const std::string& key, const json& value) {
  json& parent = GetNode(input_json_, path);
  if (parent.is_array()) {
    try {
      parent[std::stoul(key)] = value;
    } catch (...) {}
  } else {
    parent[key] = value;
  }
}

void JsonEditor::ExecuteAddKey(const std::vector<std::string>& path, const std::string& key, const json& value) {
  GetNode(input_json_, path)[key] = value;
}

void JsonEditor::ExecuteRemoveKey(const std::vector<std::string>& path, const std::string& key) {
  GetNode(input_json_, path).erase(key);
}

void JsonEditor::ExecuteAddArrayElement(const std::vector<std::string>& path, const json& value) {
  GetNode(input_json_, path).push_back(value);
}

void JsonEditor::ExecuteRemoveLastArrayElement(const std::vector<std::string>& path) {
  json& arr = GetNode(input_json_, path);
  if (!arr.empty()) arr.erase(arr.size() - 1);
}

void JsonEditor::ExecuteInsertArrayElement(const std::vector<std::string>& path, int index, const json& value) {
  json& arr = GetNode(input_json_, path);
  if (arr.is_array()) {
    auto iter = arr.begin();
    if (index <= arr.size()) {
      arr.insert(iter + index, value);
    }
  }
}

void JsonEditor::ExecuteRemoveArrayElement(const std::vector<std::string>& path, int index) {
  json& arr = GetNode(input_json_, path);
  if (arr.is_array() && index < arr.size()) {
    arr.erase(index);
  }
}

void JsonEditor::ExecuteRenameKey(const std::vector<std::string>& path, const std::string& old_key, const std::string& new_key) {
  json& node = GetNode(input_json_, path);
  node[new_key] = node[old_key];
  node.erase(old_key);
}

json& JsonEditor::GetNode(json& root, const std::vector<std::string>& path) const {
  json* node = &root;
  for (const auto& key_or_index : path) {
    try {
      if (node->is_object()) {
        node = &(*node)[key_or_index];
      } else if (node->is_array()) {
        size_t index = std::stoul(key_or_index);
        node = &(*node)[index];
      }
    } catch (...) {
      return root;
    }
  }
  return *node;
}

std::string JsonEditor::CleanStringForJson(std::string str) const {
  str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
  return str;
}

std::string JsonEditor::GetCurrentSelectionKey() {
  if (entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= entries_.size()) {
    return "[None]";
  }
  return entries_[selected_tree_item_index_].key;
}

json* JsonEditor::GetCurrentSelectedNode(std::string& out_key) const {
  if (entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= entries_.size()) {
    out_key = "[None]";
    return nullptr;
  }
  const TreeEntry& entry = entries_[selected_tree_item_index_];
  if (entry.key == "..") {
    out_key = "..";
    return nullptr;
  }
  out_key = entry.key;
  json& parent_node = GetNode(input_json_, current_path_);
  if (parent_node.is_array()) {
    try {
      return &parent_node[std::stoul(out_key)];
    } catch (...) {
      return nullptr;
    }
  } else if (parent_node.is_object()) {
    if (parent_node.contains(out_key)) {
      return &parent_node[out_key];
    }
  }
  return nullptr;
}

int JsonEditor::GetIndexFromEntries(const std::string& key) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].key == key) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

Decorator JsonEditor::GetColorFromType(const json::value_t type) const {
  switch (type) {
    case json::value_t::array:            return color(Color::MagentaLight);
    case json::value_t::boolean:          return color(Color::YellowLight);
    case json::value_t::null:             return color(Color::Red);
    case json::value_t::number_float:
    case json::value_t::number_unsigned:
    case json::value_t::number_integer:   return color(Color::Blue);
    case json::value_t::object:           return color(Color::Cyan);
    case json::value_t::string:           return color(Color::Green);
    case json::value_t::discarded:        return color(Color::GrayLight);
  }
}

ButtonOption JsonEditor::GetModalButtonOption() const {
  auto option = ButtonOption::Simple();
  option.transform = [](const EntryState& s) {
    auto element = text(s.label);
    if (s.focused) {
      element |= inverted;
    }
    return element | center | border;
  };
  return option;
}

