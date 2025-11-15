#pragma once

#include <ftxui/component/component.hpp>
#include <vector>
#include <string>
#include <functional> 

using namespace ftxui;

class BreadcrumbComponent : public ComponentBase {
public:
  BreadcrumbComponent(
    const std::vector<std::string>& initial_entries,
    std::function<void(int)> on_select)
    : on_select_(on_select) {
    
    breadcrumb_container_ = Container::Horizontal({});
    Add(breadcrumb_container_); 

    SetEntries(initial_entries);
  }

  void SetEntries(const std::vector<std::string>& new_entries) {
    entries_ = new_entries;
    breadcrumb_container_->DetachAllChildren();
    for (int i = 0; i < entries_.size(); ++i) {
      auto button = Button(entries_[i], [this, i]() {
        if (on_select_) {
          on_select_(i);
        }
      }, MakeFlatButtonOption());
      breadcrumb_container_->Add(button);
    }
  }

  Element OnRender() override {
    std::vector<Element> elements;
    int num_children = breadcrumb_container_->ChildCount();
    for (int i = 0; i < num_children; ++i) {
      elements.push_back(breadcrumb_container_->ChildAt(i)->Render());
      if (i < num_children - 1) {
        elements.push_back(text(" > ") | dim);
      }
    }
    return hbox(elements);
  }

private:
  ftxui::ButtonOption MakeFlatButtonOption() const {
    ftxui::ButtonOption flat_button_option;
    flat_button_option.transform = [](const EntryState& state) {
      auto element = text(state.label);
      if (state.focused) {
        (element |= bold) |= underlined;
      }
      if (state.active) {
        element |= inverted;
      }
      return element;
    };
    return flat_button_option;
  }

  ftxui::Component breadcrumb_container_; 
  std::vector<std::string> entries_;
  std::function<void(int)> on_select_;
};

