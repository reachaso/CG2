#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"

/// @brief 天球（Skydome）の描画を担当するコンポーネント
/// RC::GenerateSkydomeEx() 等で生成されたハンドルを保持します。
class SkydomeComponent : public IComponent {
public:
  /// @brief 天球ハンドル
  int skydomeHandle = -1;

  /// @brief 描画の可視性フラグ
  bool visible = true;

  /// @brief 色（乗算カラー）
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  /// @brief テクスチャのオーバーライド (-1 でデフォルト)
  int texOverride = -1;

  /// @brief 有効なハンドルが設定されているか確認
  /// @return 有効なハンドルを保持していれば true
  bool HasSkydome() const { return skydomeHandle >= 0; }
};
