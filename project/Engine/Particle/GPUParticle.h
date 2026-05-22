#pragma once
#include "ComputeShader/ComputeShader.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "SRVManager/SRVManager.h"
#include "struct.h"
#include <d3d12.h>
#include <wrl/client.h>

struct SceneContext;
class PipelineManager;

/// @class GPUParticle
/// @brief GPU 上で初期化・更新・描画を行うパーティクルシステム
/// @details Compute Shader でパーティクルデータを初期化し、
/// Vertex Shader で billboard 変換を行って DrawInstanced で描画する。
/// パーティクルデータは DEFAULT ヒープに配置され、CPU からは直接触らない。
class GPUParticle {
public:
  GPUParticle() = default;
  ~GPUParticle();

  // コピー禁止
  GPUParticle(const GPUParticle &) = delete;
  GPUParticle &operator=(const GPUParticle &) = delete;

  /// @brief 初期化（リソース作成、CS による初期化実行）
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx);

  /// @brief 終了処理
  void Finalize();

  /// @brief フレーム更新（PerView 定数バッファの更新）
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  /// @brief 描画処理
  /// @param ctx シーンコンテキスト
  /// @param cl グラフィックスコマンドリスト
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief ImGui デバッグ UI
  void DrawImGui();

private:
  static constexpr uint32_t kMaxParticles = 1024;
  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  // パーティクルバッファ（DEFAULT ヒープ、UAV 対応）
  Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
  SRVManager::Handle uavHandle_{};  ///< CS 書き込み用 UAV
  SRVManager::Handle srvHandle_{};  ///< VS 読み取り用 SRV

  // PerView 定数バッファ（UPLOAD ヒープ）
  Microsoft::WRL::ComPtr<ID3D12Resource> perViewCB_;
  GPUParticlePerView *perViewMapped_ = nullptr;

  // 板ポリ頂点バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> vbResource_;
  D3D12_VERTEX_BUFFER_VIEW vbView_{};
  uint32_t vertexCount_ = 0;

  // テクスチャ
  int texHandle_ = -1;

  // Compute Shader
  ComputeShader initCS_;

  // ブレンドモード
  BlendMode blendMode_ = kBlendModeAdd;

  // 参照保持
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  SRVManager *srvMgr_ = nullptr;

  bool initialized_ = false;
  bool visible_ = true;
  bool needsCSInit_ = false;  ///< CS 初期化を初回フレームに遅延実行するフラグ
};
