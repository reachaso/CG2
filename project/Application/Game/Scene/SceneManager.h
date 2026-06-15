#pragma once
#include "Fade/Fade.h"
#include "Scene.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RC { class CameraController; }

// 前方宣言
class ISceneState;

/// @class SceneManager
/// @brief ゲームシーンの切り替えと管理を行うクラス
/// @details State パターンを使用して、通常時・フェードアウト時・ロード時・フェードイン時の各状態を管理します。
/// シーンの登録、遷移リクエストの受付、およびメインループからの更新・描画呼び出しを統括します。
class Scene::SceneManager {
public:
  SceneManager();
  ~SceneManager();

  std::unique_ptr<Fade> fade_ = nullptr; ///< フェード演出用オブジェクト
  static inline const float kFadeTime = 1.0f; ///< デフォルトのフェード時間（秒）

public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Init(SceneContext &ctx);

  /// @brief 終了処理
  void Term();

  /// @brief 新しいシーンを登録する
  /// @param scene 登録するシーンの unique_ptr
  void Register(std::unique_ptr<Scene> scene);

  /// @brief 次のフレームでのシーン切り替えをリクエストする（フェード演出を伴う）
  /// @param name 遷移先のシーン名
  void RequestChange(const std::string &name);

  /// @brief シーンを即座に切り替える（演出なし）
  /// @param name 遷移先のシーン名
  /// @param ctx シーンコンテキスト
  void ChangeImmediately(const std::string &name, SceneContext &ctx);

  /// @brief 現在のシーンを再読み込みする（OnExit -> OnEnter）
  /// @param ctx シーンコンテキスト
  void ReloadCurrentScene(SceneContext &ctx);

  /// @brief 現在のシーンをバックアップする
  void BackupCurrentScene();
  
  /// @brief バックアップから現在のシーンを復元する
  /// @param ctx シーンコンテキスト
  void RestoreCurrentScene(SceneContext &ctx);

  /// @brief 更新処理（現在の状態に応じた Update を呼び出す）
  /// @param ctx シーンコンテキスト
  void Update(SceneContext &ctx);

  /// @brief 描画処理（現在の状態に応じた Render を呼び出す）
  /// @param ctx シーンコンテキスト
  /// @param cl グラフィックスコマンドリスト
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief マネージャーの状態（ISceneState）を遷移させる
  /// @param newState 新しい状態オブジェクト
  void ChangeState(std::unique_ptr<ISceneState> newState);

  /// @brief 現在実行中のシーン名を取得する
  /// @return シーン名
  const std::string &CurrentName() const { return currentName_; }

  /// @brief 現在アクティブなシーンのポインタを取得する
  /// @return シーンへのポインタ
  Scene* GetCurrentScene() const { return current_; }

  /// @brief Get all registered scene names
  std::vector<std::string> GetSceneNames() const {
    std::vector<std::string> names;
    for (auto& [key, _] : scenes_) {
      names.push_back(key);
    }
    return names;
  }

  /// @brief Load scenes from JSON files in directory
  void LoadScenesFromDirectory(const std::string& dirPath);

  /// @brief Create a new empty scene and register it
  bool CreateNewScene(const std::string& name, const std::string& dirPath);

  /// @brief Delete a registered scene (and its JSON file)
  bool DeleteScene(const std::string& name);

  /// @brief Save current scene to file
  bool SaveCurrentScene();

private:
  /// @brief 登録済みシーンを名前で検索する内部ヘルパー
  /// @param name シーン名
  /// @return 見つかったシーンのポインタ（なければ nullptr）
  Scene *get_(const std::string &name);

private:
  std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_; ///< 登録済みシーンのマップ
  Scene *current_ = nullptr;        ///< 現在アクティブなシーンのポインタ
  std::string currentName_;        ///< 現在アクティブなシーンの名前
  std::string requested_;          ///< 遷移リクエストされたシーン名

  std::unique_ptr<ISceneState> state_; ///< 現在のマネージャー状態（Normal, FadeOut, etc.）
  std::unique_ptr<RC::CameraController> camera_; ///< エディタカメラ（全シーン共有）

  // 状態クラスからのアクセスを許可
  friend class NormalState;
  friend class FadeOutState;
  friend class LoadingState;
  friend class FadeInState;
};
