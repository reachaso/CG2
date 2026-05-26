#pragma once
#include "ComputeShader/ComputeShader.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "SRVManager/SRVManager.h"
#include "struct.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

struct SceneContext;
class PipelineManager;
class DeferredReleaseQueue;

/// @brief パーティクルの挙動タイプ
enum class ParticleType : uint8_t {
  Default,    ///< 上方向噴出（既存）
  Explosion,  ///< 全方向放射（爆発）
  Rain,       ///< 下方向落下（雨）
  Count
};

/// @class GPUParticle
/// @brief GPU 上で初期化・更新・描画を行うパーティクルシステム（FreeList 方式）
/// @details Compute Shader でパーティクルデータを管理し、
/// FreeList で寿命切れパーティクルを再利用する。
/// EmitCS で射出、UpdateCS で更新、VS で billboard 描画。
/// パーティクルタイプを切り替えることで、Emit/Update の CS を動的に差し替え可能。
class GPUParticle {
public:
  GPUParticle() = default;
  ~GPUParticle();

  // コピー禁止
  GPUParticle(const GPUParticle &) = delete;
  GPUParticle &operator=(const GPUParticle &) = delete;

  /// @brief 初期化（リソース作成、CS による初期化実行）
  void Initialize(SceneContext &ctx);

  /// @brief 終了処理
  void Finalize();

  /// @brief フレーム更新（PerView / PerFrame 定数バッファの更新）
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj, float deltaTime);

  /// @brief 描画処理
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief ImGui デバッグ UI
  void DrawImGui();

  /// @brief 毎フレームの射出数を設定
  void SetEmitCount(uint32_t count) { emitCount_ = count; }

  /// @brief パーティクルタイプを設定する
  /// @param type 新しいパーティクルタイプ
  void SetParticleType(ParticleType type);

  /// @brief 現在のパーティクルタイプを取得する
  ParticleType GetParticleType() const { return currentType_; }

private:
  static constexpr uint32_t kMaxParticles = 1024;
  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }
  static constexpr uint32_t kParticleTypeCount = static_cast<uint32_t>(ParticleType::Count);

  /// @brief パーティクルタイプごとの Compute Shader セット
  struct ParticleCSSet {
    ComputeShader emit;    ///< 射出用 CS
    ComputeShader update;  ///< 更新用 CS
    bool ready = false;    ///< 両方の CS が初期化済みか
  };

  // パーティクルバッファ（DEFAULT ヒープ、UAV 対応）
  Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
  SRVManager::Handle uavHandle_{};  ///< CS 書き込み用 UAV (u0)
  SRVManager::Handle srvHandle_{};  ///< VS 読み取り用 SRV

  // FreeList バッファ（DEFAULT ヒープ、UAV 対応）
  Microsoft::WRL::ComPtr<ID3D12Resource> freeListBuffer_;      ///< uint32_t × kMaxParticles
  SRVManager::Handle freeListUavHandle_{};                     ///< UAV (u2)

  // FreeListIndex バッファ（DEFAULT ヒープ、UAV 対応）
  Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexBuffer_; ///< int32_t × 1
  SRVManager::Handle freeListIndexUavHandle_{};                ///< UAV (u1)

  // PerView 定数バッファ（UPLOAD ヒープ）
  Microsoft::WRL::ComPtr<ID3D12Resource> perViewCB_;
  GPUParticlePerView *perViewMapped_ = nullptr;

  // 板ポリ頂点バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> vbResource_;
  D3D12_VERTEX_BUFFER_VIEW vbView_{};
  uint32_t vertexCount_ = 0;

  // テクスチャ
  int texHandle_ = -1;

  // Compute Shader: 初期化用（タイプ共通）
  ComputeShader initCS_;

  // Compute Shader: タイプ別 Emit/Update セット
  std::array<ParticleCSSet, kParticleTypeCount> csSets_{};
  ParticleType currentType_ = ParticleType::Default;

  // PerFrame 定数バッファ（deltaTime 用、UPLOAD ヒープ）
  Microsoft::WRL::ComPtr<ID3D12Resource> perFrameCB_;
  GPUParticlePerFrame *perFrameMapped_ = nullptr;

  // ブレンドモード
  BlendMode blendMode_ = kBlendModeAdd;

  // 射出設定
  uint32_t emitCount_ = 10;  ///< 毎フレーム射出するパーティクル数

  // 参照保持
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  SRVManager *srvMgr_ = nullptr;
  DeferredReleaseQueue *deferredRelease_ = nullptr; ///< 遅延解放キュー（非所有）

  bool initialized_ = false;
  bool visible_ = true;
  bool needsCSInit_ = false;  ///< CS 初期化を初回フレームに遅延実行するフラグ
};
