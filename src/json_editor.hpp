#pragma once

#include "breadcrumbs.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <algorithm>

using namespace ftxui;
using json = nlohmann::json;

json& GetNode(json& root, const std::vector<std::string>& path) {
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

std::string GetKeyFromEntry(const std::string& entry) {
  size_t pos = entry.find(" (");
  if (pos != std::string::npos) {
    return entry.substr(0, pos);
  }
  return entry;
}

class JsonEditor {
 public:
  JsonEditor(json& data, const std::string& filename)
    : input_json_(data), filename_(filename), selected_tree_item_index_(0), selected_editor_tab_index_(0) {
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
    tree_menu_ = Menu(&tree_entries_, &selected_tree_item_index_, menu_option_);
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
    // 全コンポーネントの管理
    modal_container_ = Container::Tab({
      main_layout_,
      add_modal_,
      delete_modal_,
    }, &modal_state_);
    // 状態初期化
    UpdateTreeEntries();
    UpdateEditorPane();
    UpdateBreadcrumbComponent();
    tree_menu_->TakeFocus();
  }

  /// @brief 最終的なレンダリングコンポーネントを取得する。
  Component GetLayout() {
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
      }
      return document;
    });
  }

  /// @brief 現在のパスに基づいてツリーを更新。
  void UpdateTreeEntries() {
    tree_entries_.clear();
    if (!current_path_.empty()) {
      tree_entries_.push_back("..");
    }
    json& node = GetNode(input_json_, current_path_);
    if (node.is_object()) {
      for (auto& [key, val] : node.items()) {
        std::string entry = key;
        if (val.is_object()) entry += " (Object)";
        else if (val.is_array()) entry += " (Array)";
        tree_entries_.push_back(entry);
      }
    } else if (node.is_array()) {
      for (size_t i = 0; i < node.size(); ++i) {
        std::string entry = std::to_string(i);
        json& val = node[i];
        if (val.is_object()) entry += " (Object)";
        else if (val.is_array()) entry += " (Array)";
        tree_entries_.push_back(entry);
      }
    }
  }

  /// @brief ツリーでEnterが押されたときの処理。
  void OnTreeEnter() {
    if (tree_entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= tree_entries_.size()) return;
    std::string selected_entry = tree_entries_[selected_tree_item_index_];
    bool path_changed = false;
    if (selected_entry == "..") {
      if (!current_path_.empty()) {
        current_path_.pop_back();
        path_changed = true;
      }
    } else {
      std::string key = GetKeyFromEntry(selected_entry);
      json& node = GetNode(input_json_, current_path_);
      json& selected_node = (node.is_array()) ? node[std::stoul(key)] : node[key];
      if (selected_node.is_object() || selected_node.is_array()) {
        current_path_.push_back(key);
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

  /// @brief エディタ/ビューアの内容を更新する。
  void UpdateEditorPane() {
    editor_hint_ = "";
    std::string key;
    json* selected_node = GetCurrentSelectedNode(key);
    if (!selected_node) {
      selected_editor_tab_index_ = 0;
      viewer_content_ = (tree_entries_.empty() || GetCurrentSelectionKey() == "[None]")
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

 private:
  /// @brief メインレイアウトを構築する。
  Component BuildMainLayout() {
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
        text("[a] Add (Key/Value) | [d] Delete | [q] Quit") | dim,
      }) | borderLight;
    });
    // 全体レイアウト
    auto main_container = Container::Horizontal({
      tree_pane,
      editor_pane | flex,
    });
    auto main_layout = Container::Vertical({
      breadcrumbs_bar,
      main_container | flex,
      status_bar,
    });
    main_layout |= CatchEvent([this](Event event) {
      if (modal_state_ == 0) {
        if (event == Event::Character('a')) {
          return OnOpenAddModal();
        }
        if (event == Event::Character('d')) {
          return OnOpenDeleteModal();
        }
      }
      return false;
    });
    return main_layout;
  }

  /// @brief ビューアタブを描画する。
  Element RenderViewer() {
    return paragraph(viewer_content_);
  }

  /// @brief エディタでEnterが押された時に行う処理。
  void OnEditorEnter() {
    std::string key;
    if (!GetCurrentSelectedNode(key)) {
      tree_menu_->TakeFocus();
      return;
    }
    json& parent_node = GetNode(input_json_, current_path_);
    UpdateJsonValue(parent_node, key, editable_content_);
    tree_menu_->TakeFocus();
  }

  /// @brief JSONデータをキーと値で更新する。
  void UpdateJsonValue(json& parent_node, const std::string& key, const std::string& new_value) {
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

  /// @brief ツリーで現在選択されている項目のキーを得る。
  std::string GetCurrentSelectionKey() {
    if (tree_entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= tree_entries_.size()) {
      return "[None]";
    }
    return GetKeyFromEntry(tree_entries_[selected_tree_item_index_]);
  }

  /// @brief パンくずリストコンポーネントを更新する。
  void UpdateBreadcrumbComponent() {
    std::vector<std::string> entries{"root"};
    entries.insert(entries.end(), current_path_.begin(), current_path_.end());
    breadcrumb_component_->SetEntries(entries);
  }

  /// @brief 追加モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildAddModal() {
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
      Button("OK", [this] { OnAddSubmit(); }),
      Button("Cancel", [this] { modal_state_ = 0; tree_menu_->TakeFocus(); }),
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
        text(title) | bold,
        separator(),
        input_field,
        buttons->Render(),
      }) | border;
    });
    return ApplyModalBehavors(modal_renderer);
  }

  /// @brief 削除モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildDeleteModal() {
    auto buttons = Container::Horizontal({
      Button("Yes (Delete)", [this] { OnDeleteSubmit(); }),
      Button("No (Cancel)", [this] { modal_state_ = 0; tree_menu_->TakeFocus(); }),
    });
    auto modal_renderer = Renderer(buttons, [this, buttons] {
      return vbox({
        text("Are you sure you want to delete this item?") | bold,
        text("Item: " + GetCurrentSelectionKey()),
        text("This action cannnot be undone."),
        separator(),
        buttons->Render(),
      }) | border;
    });
    return ApplyModalBehavors(modal_renderer);
  }

  /// @brief 追加モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenAddModal() {
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

  /// @brief 追加モーダルで行う処理。
  void OnAddSubmit() {
    json& node = GetNode(input_json_, current_path_);
    int new_index = -1;
    if (node.is_object()) {
      std::string cleaned_key = CleanStringForJson(new_key_);
      if (cleaned_key.empty()) {
        editor_hint_ = "Error: Key cannot be empty.";
        add_key_input_->TakeFocus();
        return;
      }
      node[cleaned_key] = nullptr;
      std::string key_to_focus = cleaned_key;
      UpdateTreeEntries();
      // 新しいキーのインデックスを走査
      for (size_t i = 0; i < tree_entries_.size(); ++i) {
        if (GetKeyFromEntry(tree_entries_[i]) == key_to_focus) {
          new_index = static_cast<int>(i);
          break;
        }
      }
    } else if (node.is_array()) {
      std::string cleaned_value = CleanStringForJson(new_value_);
      json parsed_value;
      try {
        parsed_value = json::parse(cleaned_value);
      } catch (...) {
        parsed_value = cleaned_value;
      }
      node.push_back(parsed_value);
      UpdateTreeEntries();
      // フォーカスするのは最後の項目
      new_index = static_cast<int>(tree_entries_.size() - 1);
    } else {
      modal_state_ = 0;
      tree_menu_->TakeFocus();
      return;
    }
    RefreshTreeAndCloseModal(new_index);
  }

  /// @brief 削除モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenDeleteModal() {
    std::string key = GetCurrentSelectionKey();
    if (key == "[None]" || key == "..") {
      editor_hint_ = "Error: Cannot delete this item.";
      return false;
    }
    modal_state_ = 2;
    return true;
  }

  /// @brief 削除モーダルで行う処理。
  void OnDeleteSubmit() {
    std::string key = GetCurrentSelectionKey();
    if (key == "[None]" || key == "..") return;
    json& node = GetNode(input_json_, current_path_);
    try {
      if (node.is_object()) {
        node.erase(key);
      } else if (node.is_array()) {
        node.erase(std::stoul(key));
      }
    } catch (...) {
      editor_hint_ = "Error: Failed to delete item.";
    }
    RefreshTreeAndCloseModal(selected_tree_item_index_ - 1);
  }

  /// @brief モーダル共通の動作（Escで閉じる）を適用。
  /// @param modal 適用させるモーダル。
  /// @return 適用後のコンポーネント。
  Component ApplyModalBehavors(Component modal) {
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

  /// @brief ツリーを更新→インデックスにフォーカスを当てる→モーダルを閉じる。
  /// @param focus_index フォーカスを当てるインデックス。
  void RefreshTreeAndCloseModal(int focus_index) {
    UpdateTreeEntries();
    if (focus_index < 0 || focus_index >= tree_entries_.size()) {
      focus_index = 0;
    }
    selected_tree_item_index_ = focus_index;
    UpdateEditorPane();
    tree_menu_->TakeFocus();
    modal_state_ = 0;
  }

  /// @brief 文字列から改行文字を削除する。
  /// @param str 対象の文字列。
  /// @return 削除後の文字列。
  std::string CleanStringForJson(std::string str) {
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    return str;
  }

  /// @brief 現在ツリーで選択されているノードへのポインタとキー/インデックスを得る。
  /// @param[out] out_key 選択されたキー/インデックスが格納される。
  /// @return ノードへのポインタ。選択不可の場合はnullptr。
  json* GetCurrentSelectedNode(std::string& out_key) const {
    if (tree_entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= tree_entries_.size()) {
      out_key = "[None]";
      return nullptr;
    }
    std::string selected_entry = tree_entries_[selected_tree_item_index_];
    if (selected_entry == "..") {
      out_key = "..";
      return nullptr;
    }
    out_key = GetKeyFromEntry(selected_entry);
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

  json& input_json_;
  std::string filename_;
  int selected_tree_item_index_;
  int selected_editor_tab_index_;
  std::vector<std::string> current_path_;
  std::vector<std::string> tree_entries_;
  std::string viewer_content_;
  std::string editable_content_;
  std::string editor_hint_;
  // メインUI
  MenuOption menu_option_;
  Component tree_menu_;
  InputOption edit_input_option_;
  Component edit_component_;
  std::shared_ptr<BreadcrumbComponent> breadcrumb_component_;
  // モーダル
  int modal_state_;
  std::string new_key_;
  std::string new_value_;
  Component add_key_input_;
  Component add_value_input_;
  Component main_layout_;
  Component add_modal_;
  Component delete_modal_;
  Component modal_container_;
};
