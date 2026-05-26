#pragma once
#include <d3d12.h>
#include <string>

class Dx12Core;
enum class PlayState;

/// @class EditorManager
/// @brief ImGuiによるエディタUI（ドッキングレイアウト、テーマ、各種パネル）を管理するクラス
class EditorManager {
public:
  EditorManager() = default;
  ~EditorManager() = default;

  /// @brief エディタの初期化（テーマ設定など）
  void Initialize();

  /// @brief 毎フレームの更新処理（メニューバーやドッキングスペースの構築）
  /// @param core Dx12Core インスタンスへのポインタ
  void Update(Dx12Core* core = nullptr);

  /// @brief 各種パネルの描画
  /// @param viewportSrv ゲーム画面（Viewport）に表示するテクスチャのSRVハンドル
  /// @param core Dx12Core インスタンスへのポインタ
  /// @param deltaTime 前のフレームからの経過時間
  void DrawUI(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core* core = nullptr, float deltaTime = 0.0f);

  /// @brief Viewportウィンドウがホバーされているか取得する
  bool IsViewportHovered() const { return isViewportHovered_; }

  /// @brief 現在の再生状態を取得する
  PlayState GetPlayState() const;
  
  /// @brief 再生状態を設定する（外部からのリセット用）
  void SetPlayState(PlayState state);

private:
  /// @brief ダークテーマを適用する
  void ApplyDarkTheme();

  /// @brief 初回起動時やリセット時にデフォルトのドッキングレイアウトを構築する
  void SetupDockingLayout();

private:
  bool firstLayout_ = true; ///< 初回レイアウト構築フラグ
  bool resetLayout_ = false; ///< レイアウトリセット要求フラグ
  bool showDemoWindow_ = false; ///< ImGuiデモウィンドウの表示フラグ
  bool showPerfWindow_ = false; ///< パフォーマンス（FPS）ウィンドウの表示フラグ
  bool isViewportHovered_ = false; ///< Viewportウィンドウがホバーされているか
  
  PlayState playState_; ///< エディタ上での現在の再生状態

  int playIconTex_ = -1;
  int pauseIconTex_ = -1;
  int stopIconTex_ = -1;
};
