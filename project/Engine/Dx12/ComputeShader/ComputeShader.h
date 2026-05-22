#pragma once
// ============================================================================
// ComputeShader
// ----------------------------------------------------------------------------
// 汎用 Compute Shader ラッパー。
// PipelineManager に登録済みの Compute PSO / Root Signature を参照し、
// リソースバインド → Dispatch → バリア発行 を簡潔に記述できるようにする。
//
// 使用例:
//   cs.Initialize(device, pipelineManager, "skinning_cs");
//   cs.Bind(cl);
//   cs.SetSRV(0, matAddr).SetSRV(1, vtxAddr).SetUAV(0, uavHandle).SetCBV(0, cbAddr);
//   cs.Dispatch(cl, elementCount);
//   cs.UAVBarrier(cl, outputBuffer);
// ============================================================================

#include <cstdint>
#include <d3d12.h>
#include <string>

class PipelineManager;

/// @class ComputeShader
/// @brief Compute Shader の PSO/Root Signature 管理と Dispatch を簡素化するラッパー
class ComputeShader {
public:
  ComputeShader() = default;
  ~ComputeShader() = default;

  // コピー禁止、ムーブ可
  ComputeShader(const ComputeShader &) = delete;
  ComputeShader &operator=(const ComputeShader &) = delete;
  ComputeShader(ComputeShader &&) = default;
  ComputeShader &operator=(ComputeShader &&) = default;

  /// @brief PipelineManager に登録済みの Compute Pipeline を参照して初期化する
  /// @param device D3D12 デバイス
  /// @param pm PipelineManager（PSO/Root Signature の取得元）
  /// @param pipelineKey PipelineManager 上の登録キー
  /// @return 初期化成功なら true
  bool Initialize(ID3D12Device *device, PipelineManager *pm,
                  const std::string &pipelineKey);

  /// @brief 初期化済みかどうか
  bool IsReady() const { return pso_ != nullptr && root_ != nullptr; }

  // =========================================================================
  // パイプラインバインド
  // =========================================================================

  /// @brief Compute Root Signature と PSO をコマンドリストにバインドする
  /// @param cl コマンドリスト
  void Bind(ID3D12GraphicsCommandList *cl) const;

  // =========================================================================
  // リソースバインド（ビルダーパターン）
  // =========================================================================

  /// @brief SRV を Root Descriptor としてバインドする (SetComputeRootShaderResourceView)
  /// @param rootParamIndex Root Parameter インデックス
  /// @param gpuAddr GPU 仮想アドレス
  /// @return 自身への参照（チェーン呼び出し用）
  ComputeShader &SetSRV(ID3D12GraphicsCommandList *cl, uint32_t rootParamIndex,
                        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr);

  /// @brief UAV を Descriptor Table としてバインドする (SetComputeRootDescriptorTable)
  /// @param rootParamIndex Root Parameter インデックス
  /// @param gpuHandle GPU ディスクリプタハンドル
  /// @return 自身への参照
  ComputeShader &SetUAV(ID3D12GraphicsCommandList *cl, uint32_t rootParamIndex,
                        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

  /// @brief CBV を Root Descriptor としてバインドする (SetComputeRootConstantBufferView)
  /// @param rootParamIndex Root Parameter インデックス
  /// @param gpuAddr GPU 仮想アドレス
  /// @return 自身への参照
  ComputeShader &SetCBV(ID3D12GraphicsCommandList *cl, uint32_t rootParamIndex,
                        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr);

  // =========================================================================
  // Dispatch
  // =========================================================================

  /// @brief 要素数とスレッドグループサイズから自動でグループ数を計算して Dispatch する
  /// @param cl コマンドリスト
  /// @param elementCount 処理する要素の総数
  /// @param threadsPerGroup 1グループあたりのスレッド数（デフォルト: 1024）
  void Dispatch(ID3D12GraphicsCommandList *cl, uint32_t elementCount,
                uint32_t threadsPerGroup = 1024) const;

  /// @brief グループ数を直接指定して Dispatch する
  /// @param cl コマンドリスト
  /// @param groupX X 方向のグループ数
  /// @param groupY Y 方向のグループ数
  /// @param groupZ Z 方向のグループ数
  void DispatchDirect(ID3D12GraphicsCommandList *cl, uint32_t groupX,
                      uint32_t groupY = 1, uint32_t groupZ = 1) const;

  // =========================================================================
  // バリア
  // =========================================================================

  /// @brief UAV バリアを発行する（Dispatch 後の同期用）
  /// @param cl コマンドリスト
  /// @param resource バリア対象のリソース
  static void UAVBarrier(ID3D12GraphicsCommandList *cl,
                         ID3D12Resource *resource);

private:
  ID3D12PipelineState *pso_ = nullptr;   ///< Compute PSO（非所有）
  ID3D12RootSignature *root_ = nullptr;  ///< Root Signature（非所有）
};
