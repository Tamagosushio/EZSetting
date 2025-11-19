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

/// @brief ルートからのパスに基づいてjsonのノードを得る。
/// @param root ルートから始まるjsonデータ。
/// @param path 得るノードまでのパス。
/// @return jsonノードの参照。
json& GetNode(json& root, const std::vector<std::string>& path);

/// @brief ツリーのエントリーからキーを得る。
/// @param entry キーを得たいエントリー。
/// @return エントリーに対応するキー。
std::string GetKeyFromEntry(const std::string& entry);

class JsonEditor {
 public:
  /// @brief JSONエディターを構築する。
  /// @param data 編集するJSONデータ
  /// @param filename 読み込んだファイル名
  JsonEditor(json& data, const std::string& filename);

  /// @brief 最終的なレンダリングコンポーネントを取得する。
  Component GetLayout();

  /// @brief 現在のパスに基づいてツリーを更新。
  void UpdateTreeEntries();

  /// @brief ツリーでEnterが押されたときの処理。
  void OnTreeEnter();

  /// @brief エディタ/ビューアの内容を更新する。
  void UpdateEditorPane();
  
 private:
  /// @brief メインレイアウトを構築する。
  Component BuildMainLayout();
  
  /// @brief ビューアタブを描画する。
  Element RenderViewer();

  /// @brief エディタでEnterが押された時に行う処理。
  void OnEditorEnter();
  
  /// @brief JSONデータをキーと値で更新する。
  void UpdateJsonValue(json& parent_node, const std::string& key, const std::string& new_value);
  
  /// @brief ツリーで現在選択されている項目のキーを得る。
  std::string GetCurrentSelectionKey();
  
  /// @brief パンくずリストコンポーネントを更新する。
  void UpdateBreadcrumbComponent();
  
  /// @brief 追加モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildAddModal();
  
  /// @brief 削除モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildDeleteModal();
  
  /// @brief 改名モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildRenameModal();
  
  /// @brief 追加モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenAddModal();
  
  /// @brief 追加モーダルで行う処理。
  void OnAddSubmit();
  
  /// @brief 削除モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenDeleteModal();
  
  /// @brief 削除モーダルで行う処理。
  void OnDeleteSubmit();
  
  /// @brief 改名モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenRenameModal();
  
  /// @brief 改名モーダルで行う処理。
  void OnRenameSubmit();
  
  /// @brief モーダル共通の動作（Escで閉じる）を適用。
  /// @param modal 適用させるモーダル。
  /// @return 適用後のコンポーネント。
  Component ApplyModalBehavors(Component modal);
  
  /// @brief ツリーを更新→インデックスにフォーカスを当てる→モーダルを閉じる。
  /// @param focus_index フォーカスを当てるインデックス。
  void RefreshTreeAndCloseModal(int focus_index);
  
  /// @brief 文字列から改行文字を削除する。
  /// @param str 対象の文字列。
  /// @return 削除後の文字列。
  std::string CleanStringForJson(std::string str);
  
  /// @brief 現在ツリーで選択されているノードへのポインタとキー/インデックスを得る。
  /// @param[out] out_key 選択されたキー/インデックスが格納される。
  /// @return ノードへのポインタ。選択不可の場合はnullptr。
  json* GetCurrentSelectedNode(std::string& out_key) const;
  
  /// @brief キーのエントリー内インデックスを得る。
  /// @param key 検索するキー。
  /// @return キーのインデックス。なければ-1。
  int GetIndexFromEntries(const std::string& key);

  /// @brief モーダル内ボタンに適用するオプションを得る。
  /// @return ボタンオプション。
  ButtonOption GetModalButtonOption() const;

  // フィールド
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
  Component rename_key_input_;
  Component main_layout_;
  Component add_modal_;
  Component delete_modal_;
  Component rename_modal_;
  Component modal_container_;
};
