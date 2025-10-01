#pragma once
#include "CommandSystem/CommandSystem.h"
#include "DepthStencil/DepthStencil.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "FenceSync/FenceSync.h"
#include "RenderTargets/RenderTargets.h"
#include "SwapChainManager/SwapChainManager.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class DX12 {
public:
  bool Initialize(HWND hwnd, UINT width, UINT height, bool enableDebug = true);

  // フレーム制御
  void BeginFrame(float clearColor[4]);
  void EndFrame();

  // アクセサ
  ID3D12Device *Device() const { return device_.Get(); }
  ID3D12GraphicsCommandList *Cmd() const { return command_.List(); }
  UINT BackBufferIndex() const { return swapchain_.CurrentBackBufferIndex(); }

  // SRVヒープ注入（ImGuiやモデル/スプライトが使う）
  void SetSrvHeap(DescriptorHeap *srv) { srvHeap_ = srv; }
  DescriptorHeap *GetSrvHeap() const { return srvHeap_; }

private:
  // 初期化ヘルパ
  bool CreateFactoryAndDevice(bool enableDebug);

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  CommandSystem command_;
  SwapChainManager swapchain_;
  RenderTargets rtv_;
  DepthStencil dsv_;
  FenceSync fence_;

  DescriptorHeap *srvHeap_ = nullptr; // 所有しない。外部管理

  UINT rtvStride_ = 0;
};
