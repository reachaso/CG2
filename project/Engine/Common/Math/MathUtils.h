#pragma once
#include "MathTypes.h" // RC::Vector3 とかが入ってる想定
#include <algorithm>
#include <cmath>

namespace RC {

/// @brief 中心座標とサイズからAABB2Dを作成する
/// @param pos 中心座標 (x, y)
/// @param halfW 幅の半分
/// @param halfH 高さの半分
/// @return 作成されたAABB2D
inline Aabb2D MakeAabb2D(const RC::Vector3 &pos, float halfW, float halfH) {
  Aabb2D a;
  a.left = pos.x - halfW;
  a.right = pos.x + halfW;
  a.bottom = pos.y - halfH;
  a.top = pos.y + halfH;
  return a;
}

/// @brief 2つのAABB2Dが重なっているか（衝突しているか）を判定する
/// @param a AABB 1
/// @param b AABB 2
/// @return 重なっていればtrue
inline bool OverlapAabb(const Aabb2D &a, const Aabb2D &b) {
  if (a.right <= b.left)
    return false;
  if (a.left >= b.right)
    return false;
  if (a.top <= b.bottom)
    return false;
  if (a.bottom >= b.top)
    return false;
  return true;
}

// ----- scalar helpers -----

/// @brief 値を最小値と最大値の範囲内に収める
/// @param v 対象の値
/// @param mn 最小値
/// @param mx 最大値
/// @return クランプ後の値
inline float Clamp(float v, float mn, float mx) {
  return std::clamp(v, mn, mx);
}

/// @brief 値を0.0〜1.0の範囲に収める
/// @param v 対象の値
/// @return クランプ後の値
inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

/// @brief 値を0.0〜1.0の範囲に収める（Clamp01と同じ）
/// @param v 対象の値
/// @return クランプ後の値
inline float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

/// @brief 2つの値の間を線形補間する
/// @param a 開始値
/// @param b 終了値
/// @param t 補間係数 (0.0 ~ 1.0)
/// @return 補間結果
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

/// @brief シャープネスとデルタタイムから、指数スムージング用の補間係数を計算する
/// @param sharpness 追従の鋭さ（大きいほど速く追従）
/// @param dt フレーム経過時間（秒）
/// @return 補間係数 (Lerpのtとして使用可能)
inline float ExpSmoothingFactor(float sharpness, float dt) {
  if (sharpness <= 0.0f)
    return 1.0f;
  if (dt <= 0.0f)
    return 0.0f;
  return 1.0f - std::exp(-sharpness * dt);
}

// ----- Vector3 basic ops -----

/// @brief Vector3の加算
/// @param a ベクトル1
/// @param b ベクトル2
/// @return 加算結果
inline Vector3 Add(const Vector3 &a, const Vector3 &b) {
  return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

/// @brief Vector3の減算
/// @param a ベクトル1
/// @param b ベクトル2
/// @return 減算結果
inline Vector3 Sub(const Vector3 &a, const Vector3 &b) {
  return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

/// @brief Vector3のスカラー倍
/// @param v ベクトル
/// @param s 倍率
/// @return 乗算結果
inline Vector3 Mul(const Vector3 &v, float s) {
  return Vector3{v.x * s, v.y * s, v.z * s};
}

/// @brief Vector3の内積
/// @param a ベクトル1
/// @param b ベクトル2
/// @return 内積結果
inline float Dot(const Vector3 &a, const Vector3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// @brief Vector3の長さの2乗を取得
/// @param v ベクトル
/// @return 長さの2乗
inline float LengthSq(const Vector3 &v) { return Dot(v, v); }

/// @brief Vector3の長さを取得
/// @param v ベクトル
/// @return 長さ
inline float Length(const Vector3 &v) { return std::sqrt(LengthSq(v)); }

/// @brief 長さが0に近い場合にフォールバック値を返す、安全な正規化処理
/// @param v 正規化するベクトル
/// @param fallback 長さが0だった場合に返す値
/// @return 正規化後のベクトル
inline Vector3 SafeNormalize(const Vector3 &v,
                             const Vector3 &fallback = Vector3{0, 0, 1}) {
  const float lsq = LengthSq(v);
  if (lsq <= 1e-12f)
    return fallback;
  const float inv = 1.0f / std::sqrt(lsq);
  return Vector3{v.x * inv, v.y * inv, v.z * inv};
}

/// @brief Vector3の線形補間
/// @param a 開始ベクトル
/// @param b 終了ベクトル
/// @param t 補間係数 (0.0 ~ 1.0)
/// @return 補間結果
inline Vector3 Lerp(const Vector3 &a, const Vector3 &b, float t) {
  return Vector3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t};
}

/// @brief レイとAABBの交差判定 (Slab method)
/// @param ray 交差判定を行うレイ
/// @param min AABBの最小座標
/// @param max AABBの最大座標
/// @param outDistance 交差した場合の距離
/// @return 交差していればtrue
inline bool IntersectRayAABB(const Ray& ray, const Vector3& min, const Vector3& max, float& outDistance) {
  auto safe_inv = [](float d) { return d != 0.0f ? 1.0f / d : 1.0f / 1e-6f; };
  float invDx = safe_inv(ray.direction.x);
  float invDy = safe_inv(ray.direction.y);
  float invDz = safe_inv(ray.direction.z);

  float tmin = (min.x - ray.origin.x) * invDx;
  float tmax = (max.x - ray.origin.x) * invDx;
  if (tmin > tmax) std::swap(tmin, tmax);

  float tymin = (min.y - ray.origin.y) * invDy;
  float tymax = (max.y - ray.origin.y) * invDy;
  if (tymin > tymax) std::swap(tymin, tymax);

  if ((tmin > tymax) || (tymin > tmax)) return false;

  if (tymin > tmin) tmin = tymin;
  if (tymax < tmax) tmax = tymax;

  float tzmin = (min.z - ray.origin.z) * invDz;
  float tzmax = (max.z - ray.origin.z) * invDz;
  if (tzmin > tzmax) std::swap(tzmin, tzmax);

  if ((tmin > tzmax) || (tzmin > tmax)) return false;

  if (tzmin > tmin) tmin = tzmin;
  if (tzmax < tmax) tmax = tzmax;

  if (tmax < 0.0f) return false;

  outDistance = tmin >= 0.0f ? tmin : tmax;
  return true;
}

/// @brief レイとスフィア(球)の交差判定
/// @param ray 交差判定を行うレイ
/// @param center スフィアの中心座標
/// @param radius スフィアの半径
/// @param outDistance 交差した場合の距離
/// @return 交差していればtrue
inline bool IntersectRaySphere(const Ray& ray, const Vector3& center, float radius, float& outDistance) {
    Vector3 oc = Sub(ray.origin, center);
    float b = Dot(oc, ray.direction);
    float c = Dot(oc, oc) - radius * radius;
    float discriminant = b * b - c;
    if (discriminant < 0.0f) return false;

    float sqrtD = std::sqrt(discriminant);
    float t1 = -b - sqrtD;
    float t2 = -b + sqrtD;

    if (t2 < 0.0f) return false;
    outDistance = t1 >= 0.0f ? t1 : t2;
    return true;
}

} // namespace RC
