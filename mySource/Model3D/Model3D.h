#pragma once
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

class Model3D {
public:
  struct Transform {
    Vector3 scale{1, 1, 1};
    Vector3 rotate{0, 0, 0};
    Vector3 translate{0, 0, 0};
  };

  void Initialize(ID3D12Device *device);


  bool LoadObjGeometryLikeFunction(const std::string &directoryPath,
                                   const std::string &filename);

  // ビュー/プロジェクションで WVP 更新
  void Update(const Matrix4x4 &view, const Matrix4x4 &proj);

  // main 側で決めた SRV(GPU) を渡す。テクスチャは Model3D 側では生成しない。
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGpu) { srvGPU_ = srvGpu; }

  // 描画（RootParam: 0=Material,1=WVP,2=SRV,3=Light）
  void Draw(ID3D12GraphicsCommandList *cmd) const;

  // アクセス
  Transform &T() { return transform_; }
  MaterialData &Material() {
    return material_;
  } // map_Kd の textureFilePath を保持
  const MaterialData &Material() const { return material_; }

  // UV が未設定のモデルに球面マッピングを与える
  void EnsureSphericalUVIfMissing();

private:

  bool ParseObjLikeFunction_(const std::string &directoryPath,
                             const std::string &filename,
                             std::vector<VertexData> &outVertices,
                             MaterialData &outMaterial);

private:
  ID3D12Device *device_ = nullptr;

  // 頂点バッファ
  std::vector<VertexData> vertices_;
  Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
  D3D12_VERTEX_BUFFER_VIEW vbv_{};

  // CBV
  Microsoft::WRL::ComPtr<ID3D12Resource> cbMaterial_;
  Microsoft::WRL::ComPtr<ID3D12Resource> cbWVP_;
  Microsoft::WRL::ComPtr<ID3D12Resource> cbLight_;
  MaterialData material_{};
  TransformationMatrix *wvp_ = nullptr;
  DirectionalLight *light_ = nullptr;

  // SRV（main から貰うだけ）
  D3D12_GPU_DESCRIPTOR_HANDLE srvGPU_{};

  // 変換
  Transform transform_{};
  Matrix4x4 world_{}, view_{}, proj_{};
};
