#include "DepthStencil.h"

bool DepthStencil::Initialize(ID3D12Device *device, UINT width, UINT height,
                              DXGI_FORMAT dsvFormat) {
  // ヒープ
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
  heapDesc.NumDescriptors = 1;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (FAILED(device->CreateDescriptorHeap(
          &heapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf()))))
    return false;

  // リソース（深度）
  D3D12_RESOURCE_DESC tex{};
  tex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  tex.Width = width;
  tex.Height = height;
  tex.DepthOrArraySize = 1;
  tex.MipLevels = 1;
  tex.Format = DXGI_FORMAT_R24G8_TYPELESS; // SRVなし前提なら D32 などでもOK
  tex.SampleDesc = {1, 0};
  tex.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE clear{};
  clear.Format = dsvFormat; // D24_UNORM_S8_UINT 等
  clear.DepthStencil = {1.0f, 0};

  if (FAILED(device->CreateCommittedResource(
          &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
          D3D12_HEAP_FLAG_NONE, &tex, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
          IID_PPV_ARGS(depth_.ReleaseAndGetAddressOf()))))
    return false;

  D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
  dsv.Format = dsvFormat;
  dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  device->CreateDepthStencilView(
      depth_.Get(), &dsv, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
  return true;
}
