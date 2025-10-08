#pragma once
#include "function/function.h"
#include "struct.h"
#include "Math/MathTypes.h"
#include <d3d12.h>
#include <vector>

class Sphere {
public:
  Sphere() = default;
  ~Sphere();

  // Device依存リソースの作成 + 球メッシュ生成
  void Initialize(ID3D12Device *device, float radius = 0.5f,
                  UINT sliceCount = 16, UINT stackCount = 16);

  // 毎フレームの行列更新（外部カメラの View/Proj を渡す）
  void Update(const Matrix4x4 &view, const Matrix4x4 &proj);

  // 描画（RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light）
  void Draw(ID3D12GraphicsCommandList *cmdList);

  // 外からテクスチャSRV(GPUハンドル)をセット
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  // 外から調整したいとき用のアクセサ
  Transform &T() { return transform_; }
  Material *Mat() { return cbMat_.mapped; }
  DirectionalLight *Light() { return cbLight_.mapped; }

private:
  // メッシュ生成
  void BuildGeometry(float radius, UINT sliceCount, UINT stackCount);
  void UploadVB_();
  void UploadIB_();

  struct VB {
    ID3D12Resource *resource = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view{};
    uint32_t vertexCount = 0;
  };
  struct IB {
    ID3D12Resource *resource = nullptr;
    D3D12_INDEX_BUFFER_VIEW view{};
    uint32_t indexCount = 0;
  };
  struct CB_WVP {
    ID3D12Resource *resource = nullptr;
    TransformationMatrix *mapped = nullptr;
  };
  struct CB_Material {
    ID3D12Resource *resource = nullptr;
    Material *mapped = nullptr;
  };
  struct CB_Light {
    ID3D12Resource *resource = nullptr;
    DirectionalLight *mapped = nullptr;
  };

private:
  ID3D12Device *device_ = nullptr;

  // CPU側バッファ
  std::vector<VertexData> vertices_;
  std::vector<uint16_t> indices_;

  // GPUリソース
  VB vb_{};
  IB ib_{};
  CB_WVP cbWvp_{};
  CB_Material cbMat_{};
  CB_Light cbLight_{};
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};

  // 変換
  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}};
};
