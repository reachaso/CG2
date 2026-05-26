#pragma once
#include "AppConfig.h"
#include "Dx12Core.h"
#include "Game.h"
#include "Graphics/PostProcess/PostProcess.h"
#include "Graphics/Texture/RenderTexture/RenderTexture.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Editor/EditorManager.h"
#include "Input/Input.h"
#include "PipelineManager.h"
#include "Scene.h"
#include "Window/Window.h"
#include <memory>

class TitleScene;
class SelectScene;
class GameScene;
class ResultScene;
class GameOverScene;
class SampleScene;

/// @class App
/// @brief アプリケーション全体の最上位管理クラス
/// @details ウィンドウ生成、DirectX12 コア、入力システム、描画パイプライン、シーン管理などの
/// 全ての主要サブシステムを保持し、メインループ（Update/Render）を統括します。
class App {
public:
  /// @brief コンストラクタ（サブシステムのインスタンス生成のみを行う）
  App();

  /// @brief デストラクタ
  ~App();

  /// @brief アプリケーションの初期化
  /// @details ウィンドウ作成、DX12初期化、各マネージャーのセットアップ、初期シーンの構築を行います。
  /// @return 成功すれば true
  bool Init();

  /// @brief メインループの実行
  /// @details ウィンドウが閉じられるまで Update と Render を繰り返します。
  /// @return 終了コード
  int Run();

  /// @brief アプリケーションの終了処理
  void Term();

private:
  /// @brief フレーム更新処理
  void Update();

  /// @brief フレーム描画処理
  void Render();

private:
  ID3D12GraphicsCommandList *cl_ = nullptr; ///< 現在のフレームのコマンドリスト
  ID3D12Device *device_ = nullptr;            ///< D3D12 デバイス

  // 設定 & ログ
  AppConfig appConfig_; ///< アプリケーション設定（画面解像度など）

  // Window / DX12
  std::unique_ptr<Window> window_; ///< メインウィンドウ
  Dx12Core core_;                 ///< DirectX12 コアシステム
  Dx12Core::Desc coreDesc_{};      ///< DX12 初期化設定

  // 入力
  std::unique_ptr<Input> input_; ///< 入力システム

  // ImGui
  ImGuiManager imgui_; ///< ImGui 管理
  EditorManager editorManager_; ///< Editor 管理

  // パイプライン
  PipelineManager pm_; ///< グラフィックスパイプライン管理

  // オフスクリーンレンダリング / ポストプロセス
  RenderTexture renderTexture_;             ///< メイン描画用レンダーテクスチャ
  RenderTexture viewportTexture_;           ///< エディタViewport描画用レンダーテクスチャ
  std::unique_ptr<PostProcess> postProcess_; ///< ポストプロセス（グレイスケール、ガウスぼかし等）

  // === シーン管理 ===
  // === Game（シーン切替などゲーム側の責務） ===
  Game game_;             ///< ゲームロジック・シーン遷移管理
  SceneContext sceneCtx_; ///< シーン間で共有されるコンテキストデータ

  // Win32
  MSG msg_{}; ///< Win32 メッセージ構造体
};
