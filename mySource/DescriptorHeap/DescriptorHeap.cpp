#include "DescriptorHeap.h"
#include <cassert>

void DescriptorHeap::Initialize(ID3D12Device *device,
                                D3D12_DESCRIPTOR_HEAP_TYPE type,
                                uint32_t capacity, bool shaderVisible) {
  type_ = type;
  capacity_ = capacity;
  shaderVisible_ = shaderVisible;

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.NumDescriptors = capacity_;
  desc.Type = type_;
  desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
  assert(SUCCEEDED(hr));

  descriptorSize_ = device->GetDescriptorHandleIncrementSize(type_);

  cpuStart_ = heap_->GetCPUDescriptorHandleForHeapStart();
  if (shaderVisible_) {
    gpuStart_ = heap_->GetGPUDescriptorHandleForHeapStart();
  } else {
    gpuStart_.ptr = 0; // 非シェーダ可視の場合は無効
  }
  cursor_.store(0);
}

uint32_t DescriptorHeap::Allocate() {
  uint32_t index = cursor_.fetch_add(1);
  if (index >= capacity_) {
    return UINT32_MAX;
  }
  return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::Cpu(uint32_t index) const {
  D3D12_CPU_DESCRIPTOR_HANDLE h = cpuStart_;
  h.ptr += SIZE_T(index) * SIZE_T(descriptorSize_);
  return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::Gpu(uint32_t index) const {
  if (!shaderVisible_) {
    D3D12_GPU_DESCRIPTOR_HANDLE nullH{};
    nullH.ptr = 0;
    return nullH;
  }
  D3D12_GPU_DESCRIPTOR_HANDLE h = gpuStart_;
  h.ptr += UINT64(index) * UINT64(descriptorSize_);
  return h;
}
