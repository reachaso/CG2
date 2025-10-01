#include "CommandSystem.h"
#include <cassert>

bool CommandSystem::Initialize(ID3D12Device *device) {
  D3D12_COMMAND_QUEUE_DESC qdesc{};
  qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  HRESULT hr = device->CreateCommandQueue(
      &qdesc, IID_PPV_ARGS(queue_.ReleaseAndGetAddressOf()));
  if (FAILED(hr))
    return false;

  hr = device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(alloc_.ReleaseAndGetAddressOf()));
  if (FAILED(hr))
    return false;

  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 alloc_.Get(), nullptr,
                                 IID_PPV_ARGS(list_.ReleaseAndGetAddressOf()));
  if (FAILED(hr))
    return false;

  // 初回は Close しておく（毎フレーム Reset 前提のため）
  list_->Close();
  return true;
}

void CommandSystem::BeginFrame() {
  alloc_->Reset();
  list_->Reset(alloc_.Get(), nullptr);
}

void CommandSystem::Execute() {
  list_->Close();
  ID3D12CommandList *lists[] = {list_.Get()};
  queue_->ExecuteCommandLists(1, lists);
}

void CommandSystem::EndFrame() {
  // 今は特に無し（必要ならここで統一処理）
}
