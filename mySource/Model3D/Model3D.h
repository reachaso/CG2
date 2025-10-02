#pragma once
#include "../Affine/Affine.h"
#include "../function/function.h" // CreateBufferResource, LoadMaterialTemplateFile など
#include "../struct.h"
#include <d3d12.h>
#include <string>
#include <vector>

class Model3D {
public:
  Model3D() = default;
  ~Model3D();

  // Device依存リソースの作成（CB・VBなど）
  void Initialize(ID3D12Device *device);

  // 頂点配列を作り、VB を確保・転送します。
  // directoryPath: "Resources", filename: "suzanne.obj" 等
  bool LoadObjGeometryLikeFunction(const std::string &directoryPath,
                                   const std::string &filename);

  // UV未設定(=0,0)に対し簡易的な「球面UV」を付与します（Suzanne等のデモ用）
  void EnsureSphericalUVIfMissing();

  // モデルに適用するSRV(GPUハンドル)をセット
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  // 外から Transform を直接いじれるように参照を返す
  Transform &T() { return transform_; }

  // 行列更新（view/projection は外部カメラからもらう）
  void Update(const Matrix4x4 &view, const Matrix4x4 &proj);

  // 描画（RootParamは main と同じ: 0:Material, 1:WVP, 2:SRV, 3:Light）
  void Draw(ID3D12GraphicsCommandList *cmdList);

private:
  // ========== 内部ユーティリティ ==========
  struct VB {
    ID3D12Resource *resource = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view{};
    uint32_t vertexCount = 0;
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

  // OBJローダ
  bool LoadObjToVertices_(const std::string &directoryPath,
                          const std::string &filename,
                          std::vector<VertexData> &outVertices,
                          MaterialData &outMtl);

  // 頂点配列から VB を生成して転送
  void UploadVB_(const std::vector<VertexData> &vertices);

  // ========== メンバ ==========
  ID3D12Device *device_ = nullptr;
  VB vb_{};
  CB_WVP cbWvp_{};
  CB_Material cbMat_{};
  CB_Light cbLight_{};
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{}; // 外部で選択

  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}};
  MaterialData materialFile_{};

  bool visible_ = false;
};
