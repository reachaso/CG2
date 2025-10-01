#include "SwapChainManager.h"
#include <cassert>

bool SwapChainManager::Initialize(HWND hwnd, IDXGIFactory6 *factory,
                                  ID3D12CommandQueue *queue, UINT width,
                                  UINT height, DXGI_FORMAT format,
                                  UINT bufferCount) {
  factory_ = factory;
  queue_ = queue;
  format_ = format;

  DXGI_SWAP_CHAIN_DESC1 sc{};
  sc.Width = width;
  sc.Height = height;
  sc.Format = format_;
  sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sc.SampleDesc = {1, 0};
  sc.BufferCount = bufferCount;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
  HRESULT hr = factory_->CreateSwapChainForHwnd(
      queue_.Get(), hwnd, &sc, nullptr, nullptr, sc1.GetAddressOf());
  if (FAILED(hr))
    return false;

  sc1.As(&swapchain_);
  return CreateBuffers();
}

bool SwapChainManager::CreateBuffers() {
  buffers_.resize(swapchain_->GetDesc1().BufferCount);
  for (UINT i = 0; i < buffers_.size(); ++i) {
    swapchain_->GetBuffer(i,
                          IID_PPV_ARGS(buffers_[i].ReleaseAndGetAddressOf()));
  }
  return true;
}
