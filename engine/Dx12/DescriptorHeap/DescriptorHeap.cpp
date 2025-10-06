#include "DescriptorHeap.h"

void DescriptorHeap::Init(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                          UINT capacity, bool shaderVisible) {
  assert(device && capacity > 0);
  device_ = device;
  type_ = type;
  capacity_ = capacity;

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.Type = type_;
  desc.NumDescriptors = capacity_;
  desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
  assert(SUCCEEDED(hr));

  inc_ = device_->GetDescriptorHandleIncrementSize(type_);
  cpuStart_ = heap_->GetCPUDescriptorHandleForHeapStart();
  if (shaderVisible)
    gpuStart_ = heap_->GetGPUDescriptorHandleForHeapStart();
  shaderVisible_ = shaderVisible;
}

void DescriptorHeap::Term() {
  if (heap_) {
    heap_->Release();
    heap_ = nullptr;
  }
  device_ = nullptr;
  offset_ = 0;
}

// 1つ（または複数）ぶん確保して、先頭のCPUハンドルを返す
// 返り値 + i*Increment() で連番分を使えます
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AllocateCPU(UINT count) {
  assert(offset_ + count <= capacity_ && "Descriptor heap exhausted");
  D3D12_CPU_DESCRIPTOR_HANDLE h = cpuStart_;
  h.ptr += static_cast<SIZE_T>(offset_) * static_cast<SIZE_T>(inc_);
  offset_ += count;
  return h;
}

// 任意のインデックスのCPU/GPUハンドル（読み直し用）
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUAt(UINT index) const {
  assert(index < capacity_);
  D3D12_CPU_DESCRIPTOR_HANDLE h = cpuStart_;
  h.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(inc_);
  return h;
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUAt(UINT index) const {
  assert(shaderVisible_ && index < capacity_);
  D3D12_GPU_DESCRIPTOR_HANDLE h = gpuStart_;
  h.ptr += static_cast<UINT64>(index) * static_cast<UINT64>(inc_);
  return h;
}
