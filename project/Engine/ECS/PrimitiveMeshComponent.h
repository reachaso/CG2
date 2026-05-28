#pragma once

#include "IComponent.h"
#include <string>

/// @brief プリミティブメッシュの形状タイプ
enum class PrimitiveType {
  Sphere,
  Box,
  Plane,
  Cylinder,
  Cone,
  Torus,
  Capsule,
};

/// @brief プリミティブメッシュの描画を担当するコンポーネント
/// 形状タイプとサイズパラメータを保持し、Entity ループで自動描画されます。
class PrimitiveMeshComponent : public IComponent {
public:
  /// @brief プリミティブのメッシュハンドル（RC::Generate* で生成）
  int meshHandle = -1;

  /// @brief テクスチャのオーバーライド（-1 でデフォルト）
  int texOverride = -1;

  /// @brief 描画の可視性フラグ
  bool visible = true;

  /// @brief 形状タイプ（Inspector 表示用）
  PrimitiveType type = PrimitiveType::Sphere;

  /// @brief 環境マップ映り込み係数（0=映り込みなし、1=完全鏡面）
  float environmentCoeff = 0.0f;

  /// @brief 有効なメッシュが設定されているかを確認
  bool HasMesh() const { return meshHandle >= 0; }
};
