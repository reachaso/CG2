#pragma once
#pragma once
#include <d3d12.h>
#include <wrl.h>

class DepthStencil {
public:
  bool Initialize(ID3D12Device *device, UINT width, UINT height,
                  DXGI_FORMAT dsvFormat);

  ID3D12DescriptorHeap *Heap() const { return dsvHeap_.Get(); }
  D3D12_CPU_DESCRIPTOR_HANDLE Handle() const {
    return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  }
  ID3D12Resource *Resource() const { return depth_.Get(); }

private:
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
  Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
};
