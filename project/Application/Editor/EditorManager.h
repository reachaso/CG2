#pragma once
#include <d3d12.h>
#include <string>
#include <functional>
#include <memory>
#include <filesystem>
#include "Particle/GPUParticle.h"
#include "Graphics/Texture/RenderTexture/RenderTexture.h"
#include "Camera/CameraController.h"

class Dx12Core;
class Scene;
class Entity;
enum class PlayState;

/// @class EditorManager
/// @brief ImGuiによるエディタUI（ドッキングレイアウト、テーマ、各種パネル）を管理するクラス
class EditorManager {
public:
  EditorManager() = default;
  ~EditorManager() = default;

  /// @brief エディタの初期化（テーマ設定など）
  void Initialize();

  /// @brief エディタの終了処理（リソースの解放）
  void Term();

  /// @brief 毎フレームの更新処理（メニューバーやドッキングスペースの構築）
  /// @param core Dx12Core インスタンスへのポインタ
  /// @param onMenuAppend メニューバー追加用コールバック
  /// @param currentScene 現在のシーン
  void Update(Dx12Core* core = nullptr, std::function<void()> onMenuAppend = nullptr, Scene* currentScene = nullptr);

  /// @brief 各種パネルの描画
  /// @param viewportSrv ゲーム画面（Viewport）に表示するテクスチャのSRVハンドル
  /// @param core Dx12Core インスタンスへのポインタ
  /// @param pm PipelineManagerへのポインタ
  /// @param deltaTime 前のフレームからの経過時間
  /// @param currentScene 現在のシーン
  void DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core = nullptr, class PipelineManager* pm = nullptr, float deltaTime = 0.0f, Scene* currentScene = nullptr);

  /// @brief Viewportウィンドウがホバーされているか取得する
  bool IsViewportHovered() const { return isViewportHovered_; }
  void SetViewportHovered(bool hovered) { isViewportHovered_ = hovered; }

  /// @brief Particle Editor用の専用プレビュー画面を描画する
  /// @param cl 描画用コマンドリスト
  /// @param core Dx12Coreのインスタンス
  /// @param pm PipelineManagerへのポインタ
  /// @param deltaTime デルタタイム
  void RenderParticlePreview(ID3D12GraphicsCommandList* cl, Dx12Core* core, class PipelineManager* pm, float deltaTime);

  /// @brief 現在のRender Queueの状態をテキストダンプとして出力する
  void ExportRenderQueueDump();

  void SaveConfig();
  void LoadConfig();

  /// @brief 現在の再生状態を取得する
  PlayState GetPlayState() const;
  
  /// @brief 再生状態を設定する（外部からのリセット用）
  void SetPlayState(PlayState state);

  /// @brief 選択中のエンティティIDを取得する（未選択なら0）
  uint32_t GetSelectedEntityId() const;

  struct ResizeRequest {
    bool pending = false;
    int width = 0;
    int height = 0;
    bool fullscreen = false;
  };

  /// @brief ウィンドウリサイズの要求を取得
  const ResizeRequest& GetResizeRequest() const { return resizeRequest_; }
  void ClearResizeRequest() { resizeRequest_.pending = false; }

private:
  /// @brief ダークテーマを適用する
  void ApplyDarkTheme();

  /// @brief 初回起動時やリセット時にデフォルトのドッキングレイアウトを構築する
  void SetupDockingLayout();

  /// @brief ツリーノードとしてエンティティを描画する（再帰）
  void DrawEntityNode(std::shared_ptr<Entity> e, Scene* currentScene, const std::unordered_map<uint64_t, std::vector<std::shared_ptr<Entity>>>& childrenMap);

private:
  bool firstLayout_ = true; ///< 初回レイアウト構築フラグ
  bool resetLayout_ = false; ///< レイアウトリセット要求フラグ
  bool showDemoWindow_ = false; ///< ImGuiデモウィンドウの表示フラグ
  bool showPerfWindow_ = false; ///< パフォーマンス（FPS）ウィンドウの表示フラグ
  bool showRenderQueue_ = false; ///< Render Queue デバッグウィンドウの表示フラグ
  bool showParticleEditor_ = false; ///< Particle Editor ウィンドウの表示フラグ
  bool isViewportHovered_ = false; ///< Viewportウィンドウがホバーされているか
  
  PlayState playState_; ///< エディタ上での現在の再生状態

  int playIconTex_ = -1;
  int pauseIconTex_ = -1;
  int stopIconTex_ = -1;

  int eyeVisibleTex_ = -1;
  int eyeHiddenTex_ = -1;
  int lockLockedTex_ = -1;
  int lockUnlockedTex_ = -1;

  int folderIconTex_ = -1;
  int fileIconTex_ = -1;
  int fileImageTex_ = -1;
  int file3DTex_ = -1;
  int fileMaterialTex_ = -1;
  int fileDocTex_ = -1;
  int fileFontTex_ = -1;

  std::weak_ptr<Entity> selectedEntity_; ///< Inspector表示用の選択エンティティ
  uint32_t renamingEntityId_ = 0; ///< 名前変更中のエンティティID
  bool focusRename_ = false; ///< 名前変更用のフォーカスフラグ
  std::filesystem::path currentDirectory_ = "Resources"; ///< コンテンツブラウザの現在ディレクトリ

  ResizeRequest resizeRequest_; ///< ウィンドウリサイズ要求

  // --- Gizmo Settings ---
  int gizmoOperation_ = 7; // ImGuizmo::TRANSLATE
  int gizmoMode_ = 0;      // ImGuizmo::LOCAL

  // --- Particle Editor ---
  std::unique_ptr<GPUParticle> peParticle_; ///< Particle Editor 用のプレビュー用 GPUParticle
  bool peInitialized_ = false;  ///< Particle Editor が初期化済みか
  char peJsonPath_[256] = "Resources/Particle/default.json"; ///< JSON 保存/読み込みパス
  char peTexPath_[256] = "Resources/Particle/circle.png"; ///< テクスチャパス

  // プレビュー描画用リソース
  RenderTexture peRenderTexture_; ///< プレビュー描画先
  RC::CameraController peCamera_; ///< プレビュー専用カメラ
};
