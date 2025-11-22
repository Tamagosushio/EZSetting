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
#include <stack>

using namespace ftxui;
using json = nlohmann::json;

/// @brief メニュー項目が持つ情報
struct TreeEntry {
  std::string label;
  std::string key;
  json::value_t type;
};

/// @brief 操作単位
struct EditAction {
  std::function<void()> undo;
  std::function<void()> redo;
  std::vector<std::string> path;
  std::string focus_key;
};

/// @brief 履歴管理
class HistoryManager {
 public:
  /// @brief 操作を保存する。
  void Push(const EditAction& action);

  /// @brief Undo可能か。
  bool CanUndo() const;

  /// @brief Redo可能か。
  bool CanRedo() const;
  
  /// @brief Undoを実行する。
  /// @return Undoした操作。
  const EditAction* Undo();

  /// @brief Redoを実行する。
  /// @return Redoした操作。
  const EditAction* Redo();

 private:
  std::stack<EditAction> undo_stack_;
  std::stack<EditAction> redo_stack_;
};


class JsonEditor {
 public:
  /// @brief JSONエディターを構築する。
  /// @param data 編集するJSONデータ
  /// @param filename 読み込んだファイル名
  /// @param on_quit qキーによる終了処理。
  JsonEditor(json& data, const std::string& filename, std::function<void()> on_quit);

  /// @brief 最終的なレンダリングコンポーネントを取得する。
  Component GetLayout();

 private:
  /* レイアウト & レンダリング */
  /// @brief メインレイアウトを構築する。
  Component BuildMainLayout();

  /// @brief ビューアタブを描画する。
  Element RenderViewer();

  /// @brief パンくずリストコンポーネントを更新する。
  void UpdateBreadcrumbComponent();

  /* ツリー & ナビゲーション */
  /// @brief 現在のパスに基づいてツリーを更新。
  void UpdateTreeEntries();

  /// @brief ツリーでEnterが押されたときの処理。
  void OnTreeEnter();

  /* エディタ & コンテンツ */
  /// @brief エディタ/ビューアの内容を更新する。
  void UpdateEditorPane();

  /// @brief エディタでEnterが押された時に行う処理。
  void OnEditorEnter();

  /// @brief JSONデータをキーと値で更新する。
  void UpdateJsonValue(json& parent_node, const std::string& key, const std::string& new_value);

  /* モーダル */
  /// @brief 追加モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildAddModal();

  /// @brief 追加モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenAddModal();

  /// @brief 追加モーダルで行う処理。
  void OnAddSubmit();

  /// @brief 削除モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildDeleteModal();

  /// @brief 削除モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenDeleteModal();

  /// @brief 削除モーダルで行う処理。
  void OnDeleteSubmit();

  /// @brief 改名モーダルを構築する。
  /// @return モーダルのコンポーネント。
  Component BuildRenameModal();

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

  /* Undo/Redo */
  /// @brief Undo処理を実行。
  void PerformUndo();

  /// @brief Redo処理を実行。
  void PerformRedo();

  /// @brief Undo/Redo後に画面の状態を復元する。
  /// @param action 復元する操作。
  void RestoreView(const EditAction& action);

  /* ユーティリティ */
  /// @brief ルートからのパスに基づいてjsonのノードを得る。
  /// @param root ルートから始まるjsonデータ。
  /// @param path 得るノードまでのパス。
  /// @return jsonノードの参照。
  json& GetNode(json& root, const std::vector<std::string>& path) const;

  /// @brief 文字列から改行文字を削除する。
  /// @param str 対象の文字列。
  /// @return 削除後の文字列。
  std::string CleanStringForJson(std::string str) const;

  /// @brief ツリーで現在選択されている項目のキーを得る。
  std::string GetCurrentSelectionKey();

  /// @brief 現在ツリーで選択されているノードへのポインタとキー/インデックスを得る。
  /// @param[out] out_key 選択されたキー/インデックスが格納される。
  /// @return ノードへのポインタ。選択不可の場合はnullptr。
  json* GetCurrentSelectedNode(std::string& out_key) const;

  /// @brief キーのエントリー内インデックスを得る。
  /// @param key 検索するキー。
  /// @return キーのインデックス。なければ-1。
  int GetIndexFromEntries(const std::string& key) const;

  /// @brief JSONの型に対応した色を得る。
  /// @param type JSONの型。
  /// @return 色を付けるデコレーター。
  Decorator GetColorFromType(const json::value_t type) const;

  /// @brief モーダル内ボタンに適用するオプションを得る。
  /// @return ボタンオプション。
  ButtonOption GetModalButtonOption() const;

  /* フィールド */
  json& input_json_;
  std::string filename_;
  std::function<void()> on_quit_;
  HistoryManager history_manager_;
  int selected_tree_item_index_;
  int selected_editor_tab_index_;
  std::vector<std::string> current_path_;
  std::vector<TreeEntry> entries_;
  std::vector<std::string> menu_entries_;
  std::string viewer_content_;
  std::string editable_content_;
  std::string editor_hint_;

  /* メインUI */
  MenuOption menu_option_;
  Component tree_menu_;
  InputOption edit_input_option_;
  Component edit_component_;
  std::shared_ptr<BreadcrumbComponent> breadcrumb_component_;

  /* モーダル */
  int modal_state_;
  std::string new_key_;
  std::string new_value_;
  std::string rename_key_;
  Component add_key_input_;
  Component add_value_input_;
  Component rename_key_input_;
  Component main_layout_;
  Component add_modal_;
  Component delete_modal_;
  Component rename_modal_;
  Component modal_container_;
};
