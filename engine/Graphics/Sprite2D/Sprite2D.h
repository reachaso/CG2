#pragma once
#include <cstdint>
#include <d3d12.h>
#include <string>

#include "Math/Math.h" // Matrix4x4, Transform, Make* 系
#include "function/function.h" // CreateBufferResource, VertexData, Material, TransformationMatrix など

#include "imgui/imgui.h"

class Sprite2D {
public:
  Sprite2D() = default;
  ~Sprite2D();

  // 画面サイズは直交投影行列の生成に使用
  void Initialize(ID3D12Device *device, float screenWidth, float screenHeight);
  void Update();
  void Draw(ID3D12GraphicsCommandList *cmdList) const;

  // ===== setters / getters =====
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { srv_ = srv; }
  void SetScreenSize(float w, float h);
  void SetSize(float w, float h); // ピクセル指定（scale.x=w, scale.y=h）
  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }

  Transform &T() { return transform_; }  // 位置/回転Z/スケールへアクセス
  Material *Mat() { return cbMat_.map; } // 乗算カラーやuvTransform
  Matrix4x4 &UVTransform() { return cbMat_.map->uvTransform; }

  void DrawImGui(const char *name);

private:
  struct VB {
    ID3D12Resource *res = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view{};
  };
  struct IB {
    ID3D12Resource *res = nullptr;
    D3D12_INDEX_BUFFER_VIEW view{};
  };
  struct CBW {
    ID3D12Resource *res = nullptr;
    TransformationMatrix *map = nullptr;
  };
  struct CBM {
    ID3D12Resource *res = nullptr;
    Material *map = nullptr;
  };

  void Release();

private:
  ID3D12Device *device_ = nullptr;
  float screenW_ = 0.0f;
  float screenH_ = 0.0f;

  VB vb_{};
  IB ib_{};
  CBW cbWVP_{};
  CBM cbMat_{};

  D3D12_GPU_DESCRIPTOR_HANDLE srv_{};

  Matrix4x4 view_{}; // 恒等
  Matrix4x4 proj_{}; // 直交投影

  // 表示パラメータ（pxベース）
  Transform transform_{{100, 100, 1}, {0, 0, 0}, {0, 0, 0}};
  bool visible_ = true;
};
