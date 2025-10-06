#pragma once
#include <array>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>

class SwapChain {
public:
  static constexpr UINT kMaxFrames = 3;

  void Init(IDXGIFactory6 *factory, ID3D12Device *device,
            ID3D12CommandQueue *queue, HWND hwnd, UINT width, UINT height,
            DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
            UINT frameCount = 2, bool allowTearing = true);

  void Term();

  void SetRtvHeap(ID3D12DescriptorHeap *heap, UINT increment);

  void Resize(UINT width, UINT height);

  void Present(UINT syncInterval = 1, UINT flags = 0);

  // アクセサ
  UINT CurrentBackBufferIndex() const {
    return swap_->GetCurrentBackBufferIndex();
  }
  ID3D12Resource *BackBuffer(UINT i) const { return backBuffers_[i]; }
  D3D12_CPU_DESCRIPTOR_HANDLE RtvAt(UINT i) const { return rtv_[i]; }
  UINT FrameCount() const { return frameCount_; }
  DXGI_FORMAT Format() const { return format_; }
  IDXGISwapChain4 *Raw() const {
    return swap_;
  } // （ImGui Init 時などで必要なら）

private:
  void createSwapChain(HWND hwnd);

  void createBackBufferRTVs();

private:
  IDXGIFactory6 *factory_ = nullptr;    // 非所有
  ID3D12Device *device_ = nullptr;      // 非所有
  ID3D12CommandQueue *queue_ = nullptr; // 非所有
  IDXGISwapChain4 *swap_ = nullptr;     // 所有

  std::array<ID3D12Resource *, kMaxFrames> backBuffers_{};
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFrames> rtv_{};

  ID3D12DescriptorHeap *rtvHeap_ = nullptr; // 非所有
  UINT rtvInc_ = 0;

  UINT width_ = 0, height_ = 0;
  DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
  UINT frameCount_ = 2;
  bool allowTearing_ = true;
};
