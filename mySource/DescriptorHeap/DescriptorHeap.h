#pragma once
#include <atomic>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DescriptorHeap {
public:
  void Initialize(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                  uint32_t capacity, bool shaderVisible);

  // 1つ割り当ててインデックスを返す（失敗時は UINT32_MAX）
  uint32_t Allocate();

  // 汎用ハンドル取得
  D3D12_CPU_DESCRIPTOR_HANDLE Cpu(uint32_t index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE
  Gpu(uint32_t index) const; // 非シェーダ可視ヒープでは.ptr=0

  // インクリメントサイズ
  uint32_t DescriptorSize() const { return descriptorSize_; }

  // 生ポインタ欲しい時用
  ID3D12DescriptorHeap *GetHeap() const { return heap_.Get(); }

private:
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
  D3D12_DESCRIPTOR_HEAP_TYPE type_{};
  uint32_t capacity_ = 0;
  uint32_t descriptorSize_ = 0;
  std::atomic<uint32_t> cursor_{0};

  // 先頭ハンドルをキャッシュ
  D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{};
  D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{};
  bool shaderVisible_ = false;
};
