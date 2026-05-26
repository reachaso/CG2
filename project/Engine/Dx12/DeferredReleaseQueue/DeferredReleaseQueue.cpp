#include "DeferredReleaseQueue.h"

#include "Common/Log/Log.h"
#include <format>

// ============================================================================
// Enqueue
// ============================================================================

void DeferredReleaseQueue::Enqueue(
    Microsoft::WRL::ComPtr<ID3D12Resource> resource, uint64_t fenceValue) {
  if (!resource) {
    return;
  }
  queue_.push_back({std::move(resource), fenceValue});
}

// ============================================================================
// Flush
// ============================================================================

void DeferredReleaseQueue::Flush(uint64_t completedFenceValue) {
  // キューの先頭から順に、完了済みのリソースを解放する
  while (!queue_.empty()) {
    auto &front = queue_.front();
    if (front.fenceValue > completedFenceValue) {
      break; // まだ GPU が使用中
    }
    // ComPtr の Reset() で参照カウントを減らす（通常はここで解放される）
    front.resource.Reset();
    queue_.pop_front();
  }
}

// ============================================================================
// FlushAll
// ============================================================================

void DeferredReleaseQueue::FlushAll() {
  if (!queue_.empty()) {
    Log::Print(std::format(
        "[DeferredReleaseQueue] FlushAll: {} resources released",
        queue_.size()));
  }
  queue_.clear();
}
