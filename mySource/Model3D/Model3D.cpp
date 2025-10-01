#include "Model3D.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>
#include "function/function.h"
#include "Affine/Affine.h"

void Model3D::Initialize(ID3D12Device *device) {
  device_ = device;
  // Material CBV
  cbMaterial_ = CreateBufferResource(device_, sizeof(MaterialData));
  MaterialData *mapMat = nullptr;
  cbMaterial_->Map(0, nullptr, reinterpret_cast<void **>(&mapMat));
  *mapMat = material_; // 初期値
  cbMaterial_->Unmap(0, nullptr);

  // WVP CBV
  cbWVP_ = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWVP_->Map(0, nullptr, reinterpret_cast<void **>(&wvp_));
  // Light CBV
  cbLight_ = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_->Map(0, nullptr, reinterpret_cast<void **>(&light_));
  light_->direction = {0.0f, -1.0f, 0.0f};
  light_->intensity = 1.0f;
  light_->color = {1.0f, 1.0f, 1.0f};
}

bool Model3D::LoadObjGeometryLikeFunction(const std::string &dir,
                                          const std::string &file) {
  // LoadObjFile と完全同等の挙動で読み込む
  vertices_.clear();
  material_ = {};
  if (!ParseObjLikeFunction_(dir, file, vertices_, material_))
    return false;

  // VB を作って転送
  vb_ = CreateBufferResource(device_, sizeof(VertexData) * vertices_.size());
  vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
  vbv_.StrideInBytes = sizeof(VertexData);
  vbv_.SizeInBytes = UINT(sizeof(VertexData) * vertices_.size());
  if (!vertices_.empty()) {
    void *mapped = nullptr;
    vb_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, vertices_.data(),
                sizeof(VertexData) * vertices_.size());
    vb_->Unmap(0, nullptr);
  }
  return true;
}

void Model3D::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  view_ = view;
  proj_ = proj;
  world_ = MakeAffineMatrix(T().scale, T().rotate, T().translate);
  wvp_->WVP = Multiply(world_, Multiply(view_, proj_));
}

void Model3D::Draw(ID3D12GraphicsCommandList *cmd) const {
  // RootParam 0/1/3 = CBV, 2 = SRV（main 指定）
  cmd->SetGraphicsRootConstantBufferView(0,
                                         cbMaterial_->GetGPUVirtualAddress());
  cmd->SetGraphicsRootConstantBufferView(1, cbWVP_->GetGPUVirtualAddress());
  cmd->SetGraphicsRootDescriptorTable(2, srvGPU_);
  cmd->SetGraphicsRootConstantBufferView(3, cbLight_->GetGPUVirtualAddress());
  cmd->IASetVertexBuffers(0, 1, &vbv_);
  cmd->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}

bool Model3D::ParseObjLikeFunction_(const std::string &directoryPath,
                                    const std::string &filename,
                                    std::vector<VertexData> &outVertices,
                                    MaterialData &outMaterial) {
  outVertices.clear();
  outMaterial = {};
  std::ifstream file(directoryPath + "/" + filename);
  assert(file.is_open());

  std::vector<Vector4> positions;
  std::vector<Vector3> normals;
  std::vector<Vector2> texcoords;
  std::string line;

  while (std::getline(file, line)) {
    std::string id;
    std::istringstream s(line);
    s >> id;

    if (id == "v") { // 位置：x を反転
      Vector4 p;
      s >> p.x >> p.y >> p.z;
      p.x = -p.x;
      p.w = 1.0f;
      positions.push_back(p); // 【v.x反転】
      // 旧実装と同じ（x 反転 + w=1.0）【:contentReference[oaicite:5]{index=5}】
    } else if (id == "vt") { // UV：y を反転
      Vector2 t;
      s >> t.x >> t.y;
      t.y = 1.0f - t.y;
      texcoords.push_back(t); // 【vt.y反転】
      // 旧実装と同じ【:contentReference[oaicite:6]{index=6}】
    } else if (id == "vn") { // 法線：x を反転
      Vector3 n;
      s >> n.x >> n.y >> n.z;
      n.x = -n.x;
      normals.push_back(n); // 【vn.x反転】
    } else if (id == "f") { // 面：3頂点を解析→向きを反転して push
      VertexData tri[3];
      for (int vi = 0; vi < 3; ++vi) {
        std::string vdef;
        s >> vdef;
        // "v/t/n" を分解して index 化
        std::string idxStr[3] = {"", "", ""};
        size_t prev = 0, pos;
        int fld = 0;
        while (fld < 2 && (pos = vdef.find('/', prev)) != std::string::npos) {
          idxStr[fld++] = vdef.substr(prev, pos - prev);
          prev = pos + 1;
        }
        if (prev < vdef.size() && fld < 3)
          idxStr[fld++] = vdef.substr(prev);
        int pi = std::stoi(idxStr[0]);
        int ti = (!idxStr[1].empty()) ? std::stoi(idxStr[1]) : 0;
        int ni = (!idxStr[2].empty()) ? std::stoi(idxStr[2]) : 0;

        Vector4 pos4 = positions[pi - 1];
        Vector2 tex = (ti > 0 && ti - 1 < (int)texcoords.size())
                          ? texcoords[ti - 1]
                          : Vector2{0.0f, 0.0f};
        Vector3 nor = (ni > 0 && ni - 1 < (int)normals.size())
                          ? normals[ni - 1]
                          : Vector3{0.0f, 0.0f, 0.0f};
        tri[vi] = {pos4, tex, nor};
      }
      // 描画向きが逆なので 2,1,0 の順で
      outVertices.push_back(tri[2]);
      outVertices.push_back(tri[1]);
      outVertices.push_back(tri[0]);
    } else if (id == "mtllib") {
      std::string mtl;
      s >> mtl;
      // --- 最小実装（map_Kd のみ） ---
      std::ifstream mtlFile(directoryPath + "/" + mtl);
      std::string ml;
      while (std::getline(mtlFile, ml)) {
        std::string mid;
        std::istringstream ms(ml);
        ms >> mid;
        if (mid == "map_Kd") {
          std::string tex;
          ms >> tex;
          outMaterial.textureFilePath = directoryPath + "/" + tex;
        }
      }
    }
  }
  return true;
}

void Model3D::EnsureSphericalUVIfMissing() {
  bool hasUV = false;
  for (auto &v : vertices_)
    if (v.texcoord.x != 0.f || v.texcoord.y != 0.f) {
      hasUV = true;
      break;
    }
  if (hasUV)
    return;
  const float PI = 3.14159265358979323846f;
  for (auto &v : vertices_) {
    Vector3 p{v.position.x, v.position.y, v.position.z};
    float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    float theta = std::atan2(p.z, p.x);
    float phi = std::acos(p.y / len);
    v.texcoord.x = 0.5f + (theta / (2.0f * PI));
    v.texcoord.y = phi / PI;
  }
}
