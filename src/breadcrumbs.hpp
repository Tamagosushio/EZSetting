#pragma once

#include <ftxui/component/component.hpp>
#include <vector>
#include <string>
#include <functional> 

using namespace ftxui;

class BreadcrumbComponent : public ComponentBase {
public:
  /// @brief パンくずリストのコンポーネントを構築。
  /// @param initial_entries 初期エントリー。
  /// @param on_select 引数を選択インデックスに取る、選択時の処理。
  BreadcrumbComponent(const std::vector<std::string>& initial_entries, std::function<void(int)> on_select);

  /// @brief エントリーを上書きセット。
  /// @param new_entries 上書きするエントリー。
  void SetEntries(const std::vector<std::string>& new_entries);

  /// @brief レンダリング処理。
  /// @return パンくずリストのエレメント。
  Element OnRender() override;

private:
  /// @brief パンくずリストの要素となるボタンのオプションを生成。
  /// @return ボタンのオプション。
  ftxui::ButtonOption MakeFlatButtonOption() const;

  // フィールド
  ftxui::Component breadcrumb_container_; 
  std::vector<std::string> entries_;
  std::function<void(int)> on_select_;
};

