#pragma once
#include <cstdint>

// クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

// printf関数の表示位置
static const int kRowHeight = 20;
static const int kColumnWidth = 60;

static const int kWindowWidth = 1280;
static const int kWindowHeight = 720;

struct Vector2 {
  float x;
  float y;
};

struct Vector3 {
  float x;
  float y;
  float z;
};

struct Vector4 {
  float x;
  float y;
  float z;
  float w;
};

struct Matrix3x3 {
  float m[3][3];
};

struct Matrix4x4 {
  float m[4][4];
};

struct Transform {
  Vector3 scale;
  Vector3 rotation;
  Vector3 translation;
};

// 回転軸の定義
enum ShaftType {
	X, Y, Z 
};

struct VertexData {
  Vector4 Position; // 頂点の位置
  Vector2 Texcoord; // テクスチャ座標
};
