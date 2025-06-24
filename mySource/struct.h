#pragma once
#include <cstdint>
#include <vector>
#include <string>

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
enum ShaftType { X, Y, Z };

// 球
struct SphereData {
  Vector3 center = {0.0f, 0.0f, 0.0f};
  float radius = 1.0f;
  uint32_t color = 0x000000FF;
};

// 三角形
struct Triangle {
  Vector3 vertices[3] = {
      {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
  uint32_t color = 0x000000FF;
};

// 軸並行境界箱 (Axis-Aligned Bounding Box)
struct AABB {
  Vector3 min = {-1.0f, -1.0f, -1.0f};
  Vector3 max = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

// 線
// origin: 始点
// diff: 終点

struct Line {
  Vector3 origin = {0.0f, 0.0f, 0.0f};
  Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

struct Ray {
  Vector3 origin = {0.0f, 0.0f, 0.0f};
  Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

struct Segment {
  Vector3 origin = {0.0f, 0.0f, 0.0f};
  Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

// 平面
struct Plane {
  Vector3 normal = {0.0f, 1.0f, 0.0f};
  float distance = 1.0f;
  uint32_t color = 0xFFFFFFFF;
};

struct VertexData {
  Vector4 Position; // 頂点の位置
  Vector2 Texcoord; // テクスチャ座標
  Vector3 Normal;   // 法線ベクトル
};

struct Material {
  Vector4 color; // 色 (RGBA)
  int enableLighting; // ライティングの有効化フラグ
  float padding[3];
  Matrix4x4 uvTransform; // UV変換行列
};

struct TransformationMatrix {
  Matrix4x4 WVP;
  Matrix4x4 World;
};

struct DirectionalLight {
  Vector4 color; // 光の色 (RGBA)
  Vector3 direction; // 光の方向
  float intensity;   // 光の強度
};

struct MaterialData {
  std::string textureFilePath; // テクスチャファイルのパス
};

struct ModelData {
  std::vector<VertexData> vertices; // 頂点データの配列
  MaterialData material;            // マテリアルデータ
};
