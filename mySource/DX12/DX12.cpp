#include "DX12.h"
#include <assert.h>
#include <d3d12sdklayers.h>
#include "../../externals/DirectXTex/d3dx12.h"

bool DX12::Initialize(HWND hwnd, UINT width, UINT height, bool enableDebug) {
  if (!CreateFactoryAndDevice(enableDebug))
    return false;

  rtvStride_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  if (!command_.Initialize(device_.Get()))
    return false;
  if (!swapchain_.Initialize(hwnd, factory_.Get(), command_.Queue(), width,
                             height, DXGI_FORMAT_R8G8B8A8_UNORM, 2))
    return false;

  if (!rtv_.Initialize(device_.Get(), swapchain_.Format(), rtvStride_,
                       swapchain_.BufferCount()))
    return false;
  rtv_.BuildFromSwapChain(device_.Get(), swapchain_);

  if (!dsv_.Initialize(device_.Get(), width, height,
                       DXGI_FORMAT_D24_UNORM_S8_UINT))
    return false;
  if (!fence_.Initialize(device_.Get()))
    return false;

  return true;
}

void DX12::BeginFrame(float clearColor[4]) {
  command_.BeginFrame();

  // バリア: Present -> RenderTarget
  auto *cmd = command_.List();
  UINT i = BackBufferIndex();
  CD3DX12_RESOURCE_BARRIER toRT = CD3DX12_RESOURCE_BARRIER::Transition(
      swapchain_.Buffer(i), D3D12_RESOURCE_STATE_PRESENT,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  cmd->ResourceBarrier(1, &toRT);

  // RT/DS 設定 & クリア
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_.Handle(i);
  D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsv_.Handle();
  cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
  cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
  cmd->ClearDepthStencilView(dsv,
                             D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                             1.0f, 0, 0, nullptr);

  // SRVヒープ（注入されていればセット）
  if (srvHeap_) {
    ID3D12DescriptorHeap *heaps[] = {srvHeap_->GetHeap()};
    cmd->SetDescriptorHeaps(1, heaps);
  }
}

void DX12::EndFrame() {
  auto *cmd = command_.List();
  UINT i = BackBufferIndex();

  // バリア: RenderTarget -> Present
  CD3DX12_RESOURCE_BARRIER toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
      swapchain_.Buffer(i), D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PRESENT);
  cmd->ResourceBarrier(1, &toPresent);

  // 提出 & Present & フェンス待ち
  command_.Execute();
  swapchain_.SwapChain()->Present(1, 0);

  fence_.Signal(command_.Queue());
  fence_.Wait();

  command_.EndFrame();
}

bool DX12::CreateFactoryAndDevice(bool enableDebug) {
#ifdef _DEBUG
  if (enableDebug) {
    Microsoft::WRL::ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(
            IID_PPV_ARGS(dbg.ReleaseAndGetAddressOf())))) {
      dbg->EnableDebugLayer();
    }
  }
#endif

  UINT flags = 0;
#ifdef _DEBUG
  if (enableDebug)
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  if (FAILED(CreateDXGIFactory2(
          flags, IID_PPV_ARGS(factory_.ReleaseAndGetAddressOf()))))
    return false;

  // アダプタ選択（簡易: 0番）→ 必要なら高性能選択に変更可
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  factory_->EnumAdapters1(0, adapter.GetAddressOf());
  D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1,
                    IID_PPV_ARGS(device_.ReleaseAndGetAddressOf()));
  return device_ != nullptr;
}
