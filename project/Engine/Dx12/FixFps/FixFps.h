#pragma once
#include <chrono>
#include <thread>

/// @brief フレームレートを固定(60FPS)するためのクラス
class FixFps {
public:
  /// @brief 初期化（基準時間をリセット）
  void Initialize();

  /// @brief フレームレート調整。前フレームからの経過時間を計測し、必要に応じて待機します。
  /// Presentおよびフェンス待ちの直後に毎フレーム呼び出してください。
  void Update();

  /// @brief 目標FPSを設定します。
  void SetTargetFps(float fps) { targetFps_ = fps; }
  float GetTargetFps() const { return targetFps_; }

private:
  std::chrono::steady_clock::time_point reference_; ///< フレーム開始時の基準時間
  float targetFps_ = 60.0f;                         ///< 目標フレームレート
};
