#include "SwapChain.h"

void SwapChain::Init(IDXGIFactory6 *factory, ID3D12Device *device,
                     ID3D12CommandQueue *queue, HWND hwnd, UINT width,
                     UINT height, DXGI_FORMAT format, UINT frameCount,
                     bool allowTearing) {
  assert(factory && device && queue && hwnd);
  factory_ = factory;
  device_ = device;
  queue_ = queue;
  width_ = width;
  height_ = height;
  format_ = format;
  frameCount_ = (frameCount >= 1 && frameCount <= kMaxFrames) ? frameCount : 2;
  allowTearing_ = allowTearing;
  createSwapChain(hwnd);
  createBackBufferRTVs();
}

void SwapChain::Term() {
  for (UINT i = 0; i < frameCount_; ++i) {
    if (backBuffers_[i]) {
      backBuffers_[i]->Release();
      backBuffers_[i] = nullptr;
    }
  }
  if (swap_) {
    swap_->Release();
    swap_ = nullptr;
  }
  rtvHeap_ = nullptr;
  device_ = nullptr;
  queue_ = nullptr;
  factory_ = nullptr;
}

void SwapChain::SetRtvHeap(ID3D12DescriptorHeap *heap, UINT increment) {
  rtvHeap_ = heap;
  rtvInc_ = increment;
}

void SwapChain::Resize(UINT width, UINT height) {
  if (!swap_)
    return;
  width_ = width;
  height_ = height;
  // 旧バックバッファを解放
  for (UINT i = 0; i < frameCount_; ++i) {
    if (backBuffers_[i]) {
      backBuffers_[i]->Release();
      backBuffers_[i] = nullptr;
    }
  }
  UINT flags = allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
  HRESULT hr =
      swap_->ResizeBuffers(frameCount_, width_, height_, format_, flags);
  assert(SUCCEEDED(hr));
  createBackBufferRTVs();
}

void SwapChain::Present(UINT syncInterval, UINT flags) {
  swap_->Present(syncInterval, flags);
}

void SwapChain::createSwapChain(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = width_;
  desc.Height = height_;
  desc.Format = format_;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = frameCount_;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  // Tearing 対応チェック（未対応ならフラグを落とす
  BOOL tearingSupported = FALSE;
  if (SUCCEEDED(factory_->CheckFeatureSupport(
          DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported,
          sizeof(tearingSupported))) &&
      tearingSupported) { // keep allowTearing_ as is
  } else {
    allowTearing_ = false;
  }
  UINT flags = allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
  desc.Flags = flags;

  IDXGISwapChain1 *tmp = nullptr;
  HRESULT hr = factory_->CreateSwapChainForHwnd(queue_, hwnd, &desc, nullptr,
                                                nullptr, &tmp);
  assert(SUCCEEDED(hr));
  hr = tmp->QueryInterface(IID_PPV_ARGS(&swap_));
  assert(SUCCEEDED(hr));
  tmp->Release();
}

void SwapChain::createBackBufferRTVs() {
  assert(device_);
  // RTVヒープが渡されていれば、その先頭からフレーム数ぶん作る
  for (UINT i = 0; i < frameCount_; ++i) {
    HRESULT hr = swap_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
    assert(SUCCEEDED(hr));
    if (rtvHeap_) {
      D3D12_CPU_DESCRIPTOR_HANDLE h =
          rtvHeap_->GetCPUDescriptorHandleForHeapStart();
      h.ptr += static_cast<SIZE_T>(i) * static_cast<SIZE_T>(rtvInc_);
      D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
      rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      // バックバッファが UNORM のときは SRGB ビューで作る
      rtvDesc.Format = +(format_ == DXGI_FORMAT_R8G8B8A8_UNORM)
                           ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                           : format_;
      rtvDesc.Texture2D.MipSlice = 0;
      rtvDesc.Texture2D.PlaneSlice = 0;
      device_->CreateRenderTargetView(backBuffers_[i], &rtvDesc, h);
      rtv_[i] = h;
    } else {
      rtv_[i] = {};
    }
  }
}
