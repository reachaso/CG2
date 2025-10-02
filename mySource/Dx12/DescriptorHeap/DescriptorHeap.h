#pragma once
#include <cassert>
#include <cstdint>
#include <d3d12.h>

class DescriptorHeap {
public:
  void Init(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity, bool shaderVisible = false);

  void Term();

  // 1つ（または複数）ぶん確保して、先頭のCPUハンドルを返す
  // 返り値 + i*Increment() で連番分を使えます
  D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPU(UINT count = 1);

  // 任意のインデックスのCPU/GPUハンドル（読み直し用）
  D3D12_CPU_DESCRIPTOR_HANDLE CPUAt(UINT index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE GPUAt(UINT index) const;

  // 便利系
  UINT Increment() const { return inc_; }
  ID3D12DescriptorHeap *Heap() const { return heap_; }
  bool ShaderVisible() const { return shaderVisible_; }
  UINT Capacity() const { return capacity_; }
  UINT Used() const { return offset_; }
  void Reset() { offset_ = 0; } // （※後述の“フレームリング”戦略と併用推奨）

private:
  ID3D12Device *device_ = nullptr;
  D3D12_DESCRIPTOR_HEAP_TYPE type_{};
  ID3D12DescriptorHeap *heap_ = nullptr;
  UINT capacity_ = 0, inc_ = 0, offset_ = 0;
  bool shaderVisible_ = false;
  D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{};
  D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{};
};
