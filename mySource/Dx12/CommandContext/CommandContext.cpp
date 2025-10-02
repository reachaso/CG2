#include "CommandContext.h"

void CommandContext::Init(
    ID3D12Device *device,
    D3D12_COMMAND_LIST_TYPE type,
    uint32_t frameCount) {
  assert(device);
  device_ = device;
  type_ = type;
  frameCount_ = (frameCount >= 1 && frameCount <= kMaxFrames) ? frameCount : 2;

  // Queue
  D3D12_COMMAND_QUEUE_DESC qdesc{};
  qdesc.Type = type_;
  HRESULT hr = device_->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue_));
  assert(SUCCEEDED(hr));

  // Allocator(s)
  for (uint32_t i = 0; i < frameCount_; ++i) {
    hr = device_->CreateCommandAllocator(type_, IID_PPV_ARGS(&alloc_[i]));
    assert(SUCCEEDED(hr));
    frameFenceValue_[i] = 0;
  }

  // List（1本を使い回す）
  hr = device_->CreateCommandList(0, type_, alloc_[0], nullptr,
                                  IID_PPV_ARGS(&list_));
  assert(SUCCEEDED(hr));
  hr = list_->Close(); // 最初は閉じておく
  assert(SUCCEEDED(hr));

  // Fence + Event
  hr = device_->CreateFence(globalFenceValue_, D3D12_FENCE_FLAG_NONE,
                            IID_PPV_ARGS(&fence_));
  assert(SUCCEEDED(hr));
  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  assert(fenceEvent_ != nullptr);
}

void CommandContext::Term() {
  if (queue_) {
    queue_->Release();
    queue_ = nullptr;
  }
  if (list_) {
    list_->Release();
    list_ = nullptr;
  }
  for (uint32_t i = 0; i < frameCount_; ++i) {
    if (alloc_[i]) {
      alloc_[i]->Release();
      alloc_[i] = nullptr;
    }
  }
  if (fence_) {
    fence_->Release();
    fence_ = nullptr;
  }
  if (fenceEvent_) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
  device_ = nullptr;
}

// backBufferIndex を渡す
void CommandContext::BeginFrame(uint32_t frameIndex) {
  currentFrameIndex_ = frameIndex;
  WaitForFrame(frameIndex); // 同じBBの前フレームが完了していることを保証
  HRESULT hr = alloc_[frameIndex]->Reset();
  assert(SUCCEEDED(hr));
  hr = list_->Reset(alloc_[frameIndex], nullptr);
  assert(SUCCEEDED(hr));
}

void CommandContext::EndFrame() {
  HRESULT hr = list_->Close();
  assert(SUCCEEDED(hr));
  ID3D12CommandList *lists[] = {list_};
  queue_->ExecuteCommandLists(1, lists);

  const uint64_t signalValue = ++globalFenceValue_;
  hr = queue_->Signal(fence_, signalValue);
  assert(SUCCEEDED(hr));
  frameFenceValue_[currentFrameIndex_] = signalValue;
}

void CommandContext::WaitForFrame(uint32_t frameIndex) {
  const uint64_t target = frameFenceValue_[frameIndex];
  if (target == 0)
    return; // まだ投げていない
  if (fence_->GetCompletedValue() < target) {
    HRESULT hr = fence_->SetEventOnCompletion(target, fenceEvent_);
    assert(SUCCEEDED(hr));
    WaitForSingleObject(fenceEvent_, INFINITE);
  }
}

void CommandContext::FlushGPU() {
  const uint64_t v = ++globalFenceValue_;
  HRESULT hr = queue_->Signal(fence_, v);
  assert(SUCCEEDED(hr));
  if (fence_->GetCompletedValue() < v) {
    hr = fence_->SetEventOnCompletion(v, fenceEvent_);
    assert(SUCCEEDED(hr));
    WaitForSingleObject(fenceEvent_, INFINITE);
  }
}

// 便利: 単発トランジション
void CommandContext::Transition(ID3D12Resource *res,
                                D3D12_RESOURCE_STATES before,
                D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = res;
  b.Transition.StateBefore = before;
  b.Transition.StateAfter = after;
  b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  list_->ResourceBarrier(1, &b);
}
