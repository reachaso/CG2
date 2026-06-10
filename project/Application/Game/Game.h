#pragma once
#include "Scene.h"
#include "SceneManager.h"
#include "Audio/BgmManager.h"
#include "Audio/SeManager.h"
#include <string>

/// @class Game
/// @brief アプリケーション全体のメインロジックを統括するクラス
/// @details シーン遷移（SceneManager）やオーディオ（BgmManager, SeManager）の初期化と更新を管理します。
class Game {
public:
  Game() = default;
  ~Game() = default;

  /// @brief 初期化（シーンやオーディオパスの登録、初期シーンへの遷移）
  /// @param ctx シーンコンテキスト
  void Init(SceneContext &ctx);

  /// @brief 更新処理
  /// @param ctx シーンコンテキスト
  void Update(SceneContext &ctx);

  /// @brief 描画処理
  /// @param ctx シーンコンテキスト
  /// @param cl グラフィックスコマンドリスト
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief 終了処理
  void Term();

  /// @brief デバッグ用（ImGui）シーン切替 UI を表示する
  void DrawDebugUI(SceneContext &ctx);

  /// @brief 外部からシーン遷移を要求する
  /// @param name 遷移先のシーン名
  void RequestChange(const std::string &name);

  /// @brief 現在実行中のシーン名を取得する
  /// @return シーン名
  const std::string &CurrentSceneName() const;

  /// @brief 現在のシーンを再読み込みする
  /// @param ctx シーンコンテキスト
  void ReloadCurrentScene(SceneContext &ctx);

  /// @brief 現在のシーンのメモリバックアップを取る
  void BackupCurrentScene();

  /// @brief 現在のシーンをメモリバックアップから復元する
  /// @param ctx シーンコンテキスト
  void RestoreCurrentScene(SceneContext &ctx);

  /// @brief 現在実行中のシーンのポインタを取得する
  /// @return シーンのポインタ（なければnullptr）
  Scene* GetCurrentScene();

  /// @brief Provide access to SceneManager
  Scene::SceneManager& GetSceneManager() { return sceneMgr_; }

private:
  /// @brief 使用する全てのシーンを SceneManager に登録する
  void registerScenes_();

  /// @brief BGM/SE のファイルパスを一括登録する
  void registerAudioPaths_();

private:
  Scene::SceneManager sceneMgr_; ///< シーンマネージャー
  BgmManager bgm_;               ///< BGM マネージャー
  SeManager se_;                 ///< SE マネージャー

  /// @brief Directory for data-driven scene JSON files
  static inline const std::string kSceneDir = "Resources/Scenes";

  /// @brief Editor UI: new scene name input buffer
  char newSceneNameBuf_[128] = {};
};
