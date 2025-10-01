#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class RenderTargets {
public:
  bool Initialize(ID3D12Device *device, DXGI_FORMAT format, UINT rtvSize,
                  UINT bufferCount);
  void BuildFromSwapChain(ID3D12Device *device, class SwapChainManager &sc);

  ID3D12DescriptorHeap *Heap() const { return rtvHeap_.Get(); }
  D3D12_CPU_DESCRIPTOR_HANDLE Handle(UINT i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(i) * rtvStride_;
    return h;
  }

private:
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  UINT rtvStride_ = 0;
  DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
};
