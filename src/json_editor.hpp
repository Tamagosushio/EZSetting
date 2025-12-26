#pragma once

#include "breadcrumbs.hpp"
#include "json_types.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <functional>
#include <algorithm>
#include <stack>

using namespace ftxui;
using json = ordered_json;

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

  // 項目移動
  /// @brief 選択中の項目を上に移動する。
  void OnMoveUp();

  /// @brief 選択中の項目を下に移動する。
  void OnMoveDown();

  /// @brief 検索モーダルを構築する。
  Component BuildSearchModal();

  /// @brief 検索モーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenSearchModal();

  /// @brief ヘルプモーダルを構築する。
  Component BuildHelpModal();

  /// @brief ヘルプモーダルを開く処理。
  /// @return モーダルを開けたらtrue。開けなかったらfalse。
  bool OnOpenHelpModal();

  /// @brief 検索モーダルで行う処理。
  void OnSearchSubmit();

  /// @brief 検索結果を選択したときの処理。
  void OnSearchResultEnter();

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

  // Undo/Redo用のアクション実装
  /// @brief 値を編集する。
  /// @param path 親ノードへのパス。
  /// @param key 編集対象のキー。
  /// @param value 設定する値。
  void ExecuteEditValue(const std::vector<std::string>& path, const std::string& key, const json& value);

  /// @brief キーと値のペアを追加する。
  /// @param path 親ノードへのパス。
  /// @param key 追加するキー。
  /// @param value 追加する値。
  void ExecuteAddKey(const std::vector<std::string>& path, const std::string& key, const json& value);

  /// @brief キーを削除する。
  /// @param path 親ノードへのパス。
  /// @param key 削除するキー。
  void ExecuteRemoveKey(const std::vector<std::string>& path, const std::string& key);

  /// @brief 配列に要素を追加する。
  /// @param path 配列へのパス。
  /// @param value 追加する値。
  void ExecuteAddArrayElement(const std::vector<std::string>& path, const json& value);

  /// @brief 配列の最後の要素を削除する。
  /// @param path 配列へのパス。
  void ExecuteRemoveLastArrayElement(const std::vector<std::string>& path);

  /// @brief 配列の指定位置に要素を挿入する。
  /// @param path 配列へのパス。
  /// @param index 挿入するインデックス。
  /// @param value 挿入する値。
  void ExecuteInsertArrayElement(const std::vector<std::string>& path, int index, const json& value);

  /// @brief 配列の指定位置の要素を削除する。
  /// @param path 配列へのパス。
  /// @param index 削除するインデックス。
  void ExecuteRemoveArrayElement(const std::vector<std::string>& path, int index);

  /// @brief キー名を変更する。
  /// @param path 親ノードへのパス。
  /// @param old_key 変更前のキー。
  /// @param new_key 変更後のキー。
  void ExecuteRenameKey(const std::vector<std::string>& path, const std::string& old_key, const std::string& new_key);

  /// @brief キーの順序を移動する。
  /// @param path 親ノードへのパス。
  /// @param key 移動するキー。
  /// @param direction 移動方向 (-1: up, 1: down)。
  void ExecuteMoveKey(const std::vector<std::string>& path, const std::string& key, int direction);

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
  std::string search_query_;
  bool search_from_root_;
  std::vector<std::vector<std::string>> search_results_;
  int current_search_result_index_;
  std::vector<std::string> search_result_labels_;
  MenuOption search_menu_option_;
  Component add_key_input_;
  Component add_value_input_;
  Component rename_key_input_;
  Component search_input_;
  Component search_from_root_checkbox_;
  Component search_results_menu_;
  Component main_layout_;
  Component add_modal_;
  Component delete_modal_;
  Component rename_modal_;
  Component search_modal_;
  Component help_modal_;
  Component modal_container_;
};
