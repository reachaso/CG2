#pragma once
// ============================================================================
// DeferredReleaseQueue
// ----------------------------------------------------------------------------
// GPU が参照を完了するまで D3D12 リソースの解放を遅延するキュー。
// CommandContext のフェンス値を利用して同期判定を行い、
// 完了済みのリソースのみを安全に解放する。
//
// 使用例:
//   // リソースをキューに登録（所有権を移動）
//   queue.Enqueue(std::move(myResource), currentFenceValue);
//
//   // 毎フレーム呼び出して完了済みリソースを解放
//   queue.Flush(completedFenceValue);
//
//   // 終了時に全リソースを強制解放
//   queue.FlushAll();
// ============================================================================

#include <cstdint>
#include <d3d12.h>
#include <deque>
#include <wrl/client.h>

/// @class DeferredReleaseQueue
/// @brief GPU が参照を完了するまでリソースの解放を遅延するキュー
/// @details フェンス値に基づいて、GPU が使い終わったリソースだけを安全に解放する。
///          シーン遷移時やリソースの動的差し替え時にクラッシュを防止する。
class DeferredReleaseQueue {
public:
  DeferredReleaseQueue() = default;
  ~DeferredReleaseQueue() { FlushAll(); }

  // コピー禁止
  DeferredReleaseQueue(const DeferredReleaseQueue &) = delete;
  DeferredReleaseQueue &operator=(const DeferredReleaseQueue &) = delete;

  /// @brief 遅延解放キューにリソースを登録する
  /// @param resource 解放予定のリソース (ComPtr で所有権を移動)
  /// @param fenceValue このフェンス値が完了した後に解放される
  void Enqueue(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
               uint64_t fenceValue);

  /// @brief 完了済みのリソースを解放する（毎フレーム呼び出し）
  /// @param completedFenceValue 現在の完了済みフェンス値
  void Flush(uint64_t completedFenceValue);

  /// @brief 全リソースを即座に解放する（終了時用）
  void FlushAll();

  /// @brief キュー内の待機中リソース数を取得する
  /// @return 待機中のリソース数
  size_t PendingCount() const { return queue_.size(); }

private:
  /// @brief 遅延解放エントリ
  struct Entry {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource; ///< 解放待ちリソース
    uint64_t fenceValue;                             ///< 解放条件のフェンス値
  };

  std::deque<Entry> queue_; ///< FIFO キュー（フェンス値昇順）
};
