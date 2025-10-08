#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "function/function.h"
#include <array>
#include <d3d12.h>
#include <string>
#include <vector>
// -------------------------------
// 非スコープ enum: 無修飾で使える
// -------------------------------
enum LightingMode {
  None = 0,        // ライティング無し
  Lambert = 1,     // ランバート
  HalfLambert = 2, // ハーフランバート
};

class Model3D {
public:
  // -------------------------------
  // 初期/現在ライティング設定
  // -------------------------------
  struct LightingConfig {
    LightingMode mode = HalfLambert;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float dir[3] = {0.0f, -1.0f, 0.0f};
    float intensity = 1.0f;

    constexpr LightingConfig() = default;
    constexpr LightingConfig(LightingMode m) : mode(m) {}
    // intで渡されたら 0..2 にクランプして受ける
    constexpr LightingConfig(int m)
        : mode(static_cast<LightingMode>(m < 0 ? 0 : (m > 2 ? 2 : m))) {}
  };

  Model3D() = default;
  explicit Model3D(const struct LightingConfig &cfg) : initialLighting_(cfg) {}
  ~Model3D();

  // Device依存リソースの作成（CB・VBなど）
  void Initialize(ID3D12Device *device);

  // OBJ読み込み（function.hの挙動に合わせた軽量版）
  bool LoadObjGeometryLikeFunction(const std::string &directoryPath,
                                   const std::string &filename);

  // UV未設定(=0,0)に対し簡易的な球面UVを焼き込む
  void EnsureSphericalUVIfMissing();

  // モデルに適用するSRV(GPUハンドル)をセット
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  // 構造体でまとめて渡す版（宣言時 or 後から）
  Model3D &SetLightingConfig(const LightingConfig &cfg) {
    initialLighting_ = cfg;
    ApplyLightingIfReady_();
    return *this;
  }

  // 個別引数で楽に指定する版（enum / int 両対応）
  Model3D &SetLightingConfig(LightingMode mode,
                             const std::array<float, 3> &color,
                             const std::array<float, 3> &dir, float intensity);
  Model3D &SetLightingConfig(int mode, const std::array<float, 3> &color,
                             const std::array<float, 3> &dir, float intensity) {
    return SetLightingConfig(static_cast<LightingMode>(mode), color, dir,
                             intensity);
  }

  // モードだけ変えたい時（enum / int）
  Model3D &SetLightingMode(LightingMode m) {
    return SetLightingConfig(LightingConfig{m});
  }
  Model3D &SetLightingMode(int m) {
    return SetLightingConfig(LightingConfig{m});
  }

  // 外から Transform / CB を直接いじりたい場合のアクセサ
  Transform &T() { return transform_; }
  Material *Mat() { return cbMat_.mapped; }
  DirectionalLight *Light() { return cbLight_.mapped; }

  // 行列更新（view/projection は外部カメラから）
  void Update(const Matrix4x4 &view, const Matrix4x4 &proj);

  // 描画（RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light）
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

  // OBJローダ（最小移植）
  bool LoadObjToVertices_(const std::string &directoryPath,
                          const std::string &filename,
                          std::vector<VertexData> &outVertices,
                          MaterialData &outMtl);

  // 頂点配列から VB を生成しアップロード
  void UploadVB_(const std::vector<VertexData> &vertices);

  // .mtl 読み
  MaterialData LoadMaterialTemplateFile_(const std::string &directoryPath,
                                         const std::string &filename);

  // ライティング初期値/現在値をCBへ反映
  void ApplyLightingIfReady_();

private:
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

  LightingConfig initialLighting_{};
};
