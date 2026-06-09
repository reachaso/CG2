#pragma once

namespace RC {

/// @brief 2次元ベクトル
struct Vector2 {
  float x, y; ///< x, y 成分
};

/// @brief 2次元整数ベクトル (グリッド座標用)
struct Vector2Int {
  int x, y; ///< x, y 成分

  /// @brief 等価比較演算子
  /// @param other 比較対象
  /// @return 同一座標ならtrue
  bool operator==(const Vector2Int &other) const {
    return x == other.x && y == other.y;
  }
};

/// @brief 3次元ベクトル
struct Vector3 {
  float x, y, z; ///< x, y, z 成分
};

/// @brief 4次元ベクトル
struct Vector4 {
  float x, y, z, w; ///< x, y, z, w 成分
};

/// @brief 3x3行列
struct Matrix3x3 {
  float m[3][3]; ///< 3x3の要素配列
};

/// @brief 4x4行列
struct Matrix4x4 {
  float m[4][4]; ///< 4x4の要素配列
};

/// @brief 回転軸の定義
enum ShaftType {
  X, ///< X軸
  Y, ///< Y軸
  Z  ///< Z軸
};

/// @brief 2次元のAABB (Axis-Aligned Bounding Box)
struct Aabb2D {
  float left = 0;   ///< 左端座標
  float right = 0;  ///< 右端座標
  float bottom = 0; ///< 下端座標
  float top = 0;    ///< 上端座標
};

/// @brief クォータニオン (四元数)
struct Quaternion {
  float x, y, z, w; ///< x, y, z, w 成分
};

/// @brief 3次元空間のレイ（半直線）
struct Ray {
  Vector3 origin;    ///< レイの起点
  Vector3 direction; ///< レイの方向（正規化されていること）
};

} // namespace RC
