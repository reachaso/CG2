#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <string>

/// @brief 2Dスプライトの描画を担当するコンポーネント
/// スプライトのハンドルとスクリーンサイズを保持し、Entity ループで自動描画されます。
class SpriteRendererComponent : public IComponent {
public:
  /// @brief スプライトハンドル（RC::LoadSprite で生成）
  int spriteHandle = -1;

  /// @brief 描画の可視性フラグ
  bool visible = true;

  /// @brief スプライトの表示サイズ（スクリーン座標、ピクセル単位）
  RC::Vector2 size = {100.0f, 100.0f};

  /// @brief スプライトの色（乗算カラー）
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  /// @brief 有効なスプライトが設定されているか確認
  bool HasSprite() const { return spriteHandle >= 0; }
};
