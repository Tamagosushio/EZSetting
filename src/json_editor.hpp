#pragma once

#include "breadcrumbs.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
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
    
    edit_input_option_.on_enter = [this] { OnEditorEnter(); };
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
    UpdateTreeEntries();
    UpdateEditorPane();
    UpdateBreadcrumbComponent();
  }

  Component GetLayout() {
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
      }) | borderLight;
    });
    // 全体レイアウト
    auto main_container = Container::Horizontal({
      tree_pane,
      editor_pane | flex,
    });
    return Container::Vertical({
      breadcrumbs_bar,
      main_container | flex,
      status_bar,
    });
  }

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

  void UpdateEditorPane() {
    editor_hint_ = "";
    if (tree_entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= tree_entries_.size()) {
      selected_editor_tab_index_ = 0;
      viewer_content_ = "Select an item from the left.";
      return;
    }
    std::string selected_entry = tree_entries_[selected_tree_item_index_];
    if (selected_entry == "..") {
      selected_editor_tab_index_ = 0;
      viewer_content_ = "Select an item to view/edit.";
      return;
    }
    std::string key = GetKeyFromEntry(selected_entry);
    json& node = GetNode(input_json_, current_path_);
    json* selected_node = nullptr;
    if (node.is_array()) {
      try {
        selected_node = &node[std::stoul(key)];
      } catch (...) {}
    } else if (node.is_object()) {
      selected_node = &node[key];
    }
    if (!selected_node) {
      selected_editor_tab_index_ = 0;
      viewer_content_ = "Error: Node not found.";
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
  Element RenderViewer() {
    return paragraph(viewer_content_);
  }

  void OnEditorEnter() {
    std::string key = GetCurrentSelectionKey();
    if (key == "[None]" || key == "..") return;
    json& parent_node = GetNode(input_json_, current_path_);
    UpdateJsonValue(parent_node, key, editable_content_);
  }

  void UpdateJsonValue(json& parent_node, const std::string& key, const std::string& new_value) {
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
      json parsed_value = json::parse(new_value);
      *target_node = parsed_value;
    } catch (...) {
      *target_node = new_value;
    }
  }

  std::string GetCurrentSelectionKey() {
    if (tree_entries_.empty() || selected_tree_item_index_ < 0 || selected_tree_item_index_ >= tree_entries_.size()) {
      return "[None]";
    }
    return GetKeyFromEntry(tree_entries_[selected_tree_item_index_]);
  }

  void UpdateBreadcrumbComponent() {
    std::vector<std::string> entries{"root"};
    entries.insert(entries.end(), current_path_.begin(), current_path_.end());
    breadcrumb_component_->SetEntries(entries);
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
  MenuOption menu_option_;
  Component tree_menu_;
  InputOption edit_input_option_;
  Component edit_component_;
  std::shared_ptr<BreadcrumbComponent> breadcrumb_component_;
};
