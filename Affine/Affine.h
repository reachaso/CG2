#pragma once
#include "../struct.h"

//==================================
// Vector3 関連関数
//==================================

// 加算
Vector3 Add(const Vector3 &v1, const Vector3 &v2);
// 減算
Vector3 Subtract(const Vector3 &v1, const Vector3 &v2);
// スカラー倍
Vector3 Multiply(const Vector3 &v1, float scalar);
// 内積
float Dot(const Vector3 &v1, const Vector3 &v2);
// 長さ
float Length(const Vector3 &v);
// 正規化
Vector3 Normalize(const Vector3 &v);
// 座標変換
Vector3 Vector3Transform(const Vector3 &vector, const Matrix4x4 &matrix);

//==================================
// Matrix4x4 関連関数
//==================================

// 行列の加法
Matrix4x4 Add(const Matrix4x4 &m1, const Matrix4x4 &m2);
// 行列の減法
Matrix4x4 Subtract(const Matrix4x4 &m1, const Matrix4x4 &m2);
// 行列の積
Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);
// 逆行列
Matrix4x4 Inverse(const Matrix4x4 &m);
// 転置行列
Matrix4x4 Transpose(const Matrix4x4 &m);
// 単位行列の生成
Matrix4x4 MakeIdentity4x4();

//==================================
// 回転行列
//==================================

Matrix4x4 MakeRotateMatrix(ShaftType shaft, float radian);

//==================================
// 平行移動行列
//==================================

Matrix4x4 MakeTranslateMatrix(const Vector3 &translate);

//==================================
// 拡大縮小行列
//==================================

Matrix4x4 MakeScaleMatrix(const Vector3 &scale);

//==================================
// Affine関数
//==================================
Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate,
                           const Vector3 &translate);
