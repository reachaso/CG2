#pragma once

#include "IComponent.h"
#include <string>

/// @brief アニメーション制御を担当するコンポーネント
/// ModelRendererComponent と組み合わせて使用します。
/// アニメーションの再生/停止、速度制御、スケルトンのデバッグ表示を管理します。
class AnimationComponent : public IComponent {
public:
  /// @brief アニメーション再生中フラグ
  bool playing = true;

  /// @brief 再生速度（1.0 = 通常速度）
  float speed = 1.0f;

  /// @brief スケルトンのデバッグ表示フラグ
  bool showSkeleton = false;

  /// @brief 外部アニメーションファイルパス（空文字ならモデル内蔵アニメーションを使用）
  std::string animationPath;

  /// @brief アニメーションがアタッチ済みかどうか（内部管理用）
  bool attached_ = false;
};
