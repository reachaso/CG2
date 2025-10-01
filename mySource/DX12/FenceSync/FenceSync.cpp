#include "FenceSync.h"
#include <cassert>

bool FenceSync::Initialize(ID3D12Device *device) {
  if (FAILED(
          device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                              IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf()))))
    return false;
  value_ = 0;
  event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  return event_ != nullptr;
}

void FenceSync::Signal(ID3D12CommandQueue *queue) {
  ++value_;
  queue->Signal(fence_.Get(), value_);
}

void FenceSync::Wait() {
  if (fence_->GetCompletedValue() < value_) {
    fence_->SetEventOnCompletion(value_, event_);
    WaitForSingleObject(event_, INFINITE);
  }
}
