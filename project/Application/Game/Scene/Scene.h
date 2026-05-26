#pragma once
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "AppConfig.h"

// 前方宣言（App 側の実体を参照するため）
class Dx12Core;
class Input;
class DebugCamera;
class MainCamera;
class ImGuiManager;
class PipelineManager;
class BgmManager;
class SeManager;
class PostProcess;

/// @enum PlayState
/// @brief ゲームの再生状態を表す列挙型
enum class PlayState {
  Stopped, // 停止中（編集モード）
  Playing, // 再生中
  Paused   // 一時停止中
};

/// @struct SceneContext
/// @brief シーン間で共有されるエンジンコンポーネントへの参照を保持する構造体
/// @details 各シーンの Update/Render に渡され、グラフィックスデバイス、入力、オーディオ、デバッグツールなどへのアクセスを提供します。
struct SceneContext {
  Dx12Core *core = nullptr;             ///< DirectX12 コアシステム
  Input *input = nullptr;               ///< 入力システム（キーボード、マウス、コントローラー）
  AppConfig *app = nullptr;             ///< アプリケーション設定
  ImGuiManager *imgui = nullptr;         ///< ImGui 管理
  PipelineManager *pipelineManager = nullptr; ///< パイプライン管理
  PostProcess *postProcess = nullptr;     ///< ポストプロセス管理
  BgmManager *bgmManager = nullptr;      ///< BGM 管理
  SeManager *seManager = nullptr;        ///< SE 管理
  float deltaTime = 1.0f / 60.0f;        ///< 前フレームからの経過時間 (秒)
  
  PlayState playState = PlayState::Playing; ///< 現在の再生状態

  /// @brief 再生中かどうか判定する
  bool isPlaying() const {
    return playState == PlayState::Playing;
  }
};

/// @class Scene
/// @brief ゲームシーンの基底抽象クラス
/// @details 全てのゲームシーン（タイトル、ゲーム本編、リザルトなど）はこのクラスを継承して実装します。
/// シーンのライフサイクル（入場・退場・更新・描画）を定義します。
class Scene {
public:
  virtual ~Scene() = default;

  /// @brief シーン名を取得する
  /// @return シーン名（文字列リテラル）
  virtual const char *Name() const = 0;

  /// @brief シーンに遷移した瞬間に呼び出される
  /// @param ctx シーンコンテキスト
  virtual void OnEnter(SceneContext &) {}

  /// @brief シーンから離れる瞬間に呼び出される
  /// @param ctx シーンコンテキスト
  virtual void OnExit(SceneContext &) {}

  class SceneManager; ///< 前方宣言

  /// @brief シーンの更新処理
  /// @param sm シーンマネージャー（シーン遷移要求に使用）
  /// @param ctx シーンコンテキスト
  virtual void Update(SceneManager &sm, SceneContext &ctx) = 0;

  /// @brief シーンの描画処理
  /// @param ctx シーンコンテキスト
  /// @param cl グラフィックスコマンドリスト
  virtual void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) = 0;
};
