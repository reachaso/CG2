#pragma once
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class DX12 {

public:
  void Initialize();

  // HRESULTはWindows系のエラーコードであり、
  // 関数が成功したかどうかをSUCCEEDEDマクロで判定する
  HRESULT hr{};

  IDXGIAdapter4 *useAdapter = nullptr;                // 使用するアダプター
  IDXGIFactory6 *dxgiFactory = nullptr;               // DXGIファクトリー
  ID3D12Device *device = nullptr;                     // D3D12デバイス
  ID3D12CommandAllocator *commandAllocator = nullptr; // コマンドアロケータ
  ID3D12GraphicsCommandList *commandList = nullptr;   // コマンドリスト
  IDXGISwapChain4 *swapChain = nullptr;               // スワップチェーン
  ID3D12CommandQueue *commandQueue = nullptr;         // コマンドキュー
  ID3D12DescriptorHeap *rtvDescriptorHeap = nullptr;  // ディスクリプタヒープ
};
