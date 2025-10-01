#pragma once
#include <d3d12.h>
#include <wrl.h>

class CommandSystem {
public:
  bool Initialize(ID3D12Device *device);

  // フレームの頭で呼ぶ: リスト/アロケータ Reset
  void BeginFrame();
  // コマンドを閉じて、キューで実行
  void Execute();
  // 次フレームに備えてClose済みをResetしやすいように補助
  void EndFrame();

  ID3D12CommandQueue *Queue() const { return queue_.Get(); }
  ID3D12CommandAllocator *Alloc() const { return alloc_.Get(); }
  ID3D12GraphicsCommandList *List() const { return list_.Get(); }

private:
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
};
