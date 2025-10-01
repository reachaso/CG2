#pragma once
#include <dxgi1_6.h>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

class SwapChainManager {
public:
  bool Initialize(HWND hwnd, IDXGIFactory6 *factory, ID3D12CommandQueue *queue,
                  UINT width, UINT height, DXGI_FORMAT format,
                  UINT bufferCount);

  IDXGISwapChain4 *SwapChain() const { return swapchain_.Get(); }
  UINT BufferCount() const { return static_cast<UINT>(buffers_.size()); }
  ID3D12Resource *Buffer(UINT i) const { return buffers_[i].Get(); }
  UINT CurrentBackBufferIndex() const {
    return swapchain_->GetCurrentBackBufferIndex();
  }
  DXGI_FORMAT Format() const { return format_; }

  // リサイズ等で再作成したい場合に備えた雛形
  // bool Resize(...);

private:
  bool CreateBuffers();

  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain_;
  std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> buffers_;
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
};
