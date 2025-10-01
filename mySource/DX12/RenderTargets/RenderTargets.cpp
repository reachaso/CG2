#include "RenderTargets.h"
#include "../SwapChainManager/SwapChainManager.h"

bool RenderTargets::Initialize(ID3D12Device *device, DXGI_FORMAT format,
                               UINT rtvSize, UINT bufferCount) {
  format_ = format;
  rtvStride_ = rtvSize;

  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.NumDescriptors = bufferCount;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (FAILED(device->CreateDescriptorHeap(
          &desc, IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf()))))
    return false;
  return true;
}

void RenderTargets::BuildFromSwapChain(ID3D12Device *device,
                                       SwapChainManager &sc) {
  for (UINT i = 0; i < sc.BufferCount(); ++i) {
    D3D12_CPU_DESCRIPTOR_HANDLE h = Handle(i);
    device->CreateRenderTargetView(sc.Buffer(i), nullptr, h);
  }
}
