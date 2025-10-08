#pragma once
#include "CommandContext/CommandContext.h"
#include "DepthStencil/DepthStencil.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "Device/Device.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "SwapChain/SwapChain.h"
#include <d3d12.h>
#include <dxgi1_6.h>

class Dx12Core {
public:
  struct Desc {
    UINT width = 1280;
    UINT height = 720;
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    UINT frameCount = 2;
    bool debug = true;
    bool gpuValidation = false;
    UINT srvHeapCapacity = 256;
    bool allowTearingIfSupported = true;
  };

  void Init(HWND hwnd, const Desc &d);
  void Term();

  // 毎フレーム
  void BeginFrame(); // フレーム開始（allocator/list reset を内包）
  void EndFrame();   // Close→Execute→Present（Fence発行まで）
  void WaitForGPU(); // Flush（終了時など）
  UINT BackBufferIndex() const { return backIndex_; }

  // よく使うハンドル
  ID3D12Device *GetDevice() const { return device_.GetDevice(); }
  ID3D12GraphicsCommandList *CL() const { return cmd_.List(); }
  ID3D12CommandQueue *Queue() const { return cmd_.Queue(); }

  // ヒープとRT/DS
  DescriptorHeap &SRV() { return srv_; }
  DescriptorHeap &RTV() { return rtv_; }
  DescriptorHeap &DSV() { return dsv_; }
  D3D12_CPU_DESCRIPTOR_HANDLE CurrentRTV() const {
    return swap_.RtvAt(backIndex_);
  }
  D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const { return depth_.Dsv(); }

  // クリアのヘルパー（任意）
  void Clear(float r = 0.1f, float g = 0.25f, float b = 0.5f, float a = 1.0f);

  // Dx12Core.h
  UINT FrameCount() const { return swap_.FrameCount(); }

  void SetViewport(const D3D12_VIEWPORT &vp) { viewport_ = vp; }
  void SetScissor(const D3D12_RECT &rc) { scissor_ = rc; }
  const D3D12_VIEWPORT &Viewport() const { return viewport_; }
  const D3D12_RECT &Scissor() const { return scissor_; }

  void ResetViewportScissorToBackbuffer(UINT width, UINT height) {
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissor_.left = 0;
    scissor_.top = 0;
    scissor_.right = static_cast<LONG>(width);
    scissor_.bottom = static_cast<LONG>(height);
  }

private:
  Desc desc_{};
  Device device_;
  CommandContext cmd_;
  SwapChain swap_;
  DescriptorHeap rtv_, dsv_, srv_;
  DepthStencil depth_;
  UINT backIndex_ = 0;
  bool allowTearing_ = false;
  D3D12_VIEWPORT viewport_{};
  D3D12_RECT scissor_{};
};
