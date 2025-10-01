#pragma once
#include <d3d12.h>
#include <wrl.h>

class FenceSync {
public:
  bool Initialize(ID3D12Device *device);
  void Signal(ID3D12CommandQueue *queue);
  void Wait();

private:
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  HANDLE event_ = nullptr;
  UINT64 value_ = 0;
};
