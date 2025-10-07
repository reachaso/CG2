#include "Model3D.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring> // ← 追加: std::memcpy
#include <fstream>
#include <numbers>
#include <sstream>

static constexpr float kPi = std::numbers::pi_v<float>;

Model3D::~Model3D() {
  if (vb_.resource)
    vb_.resource->Release();
  if (cbWvp_.resource)
    cbWvp_.resource->Release();
  if (cbMat_.resource)
    cbMat_.resource->Release();
  if (cbLight_.resource)
    cbLight_.resource->Release();
}

void Model3D::Initialize(ID3D12Device *device) {
  device_ = device;

  // WVP CB
  cbWvp_.resource = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();

  // Material CB
  cbMat_.resource = CreateBufferResource(device_, sizeof(Material));
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->lightingMode = 2; // 既定 HalfLambert
  cbMat_.mapped->uvTransform = MakeIdentity4x4();

  // Light CB（各Modelが自前で持つ）
  cbLight_.resource = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;

  // 初期ライティング（コンストラクタで渡された値）を反映
  ApplyLightingIfReady_();
}

bool Model3D::LoadObjGeometryLikeFunction(const std::string &directoryPath,
                                          const std::string &filename) {
  std::vector<VertexData> verts;
  MaterialData mtl;
  if (!LoadObjToVertices_(directoryPath, filename, verts, mtl)) {
    return false;
  }
  materialFile_ = mtl;
  UploadVB_(verts);
  return true;
}

void Model3D::EnsureSphericalUVIfMissing() {
  if (!vb_.resource || vb_.vertexCount == 0)
    return;

  VertexData *vtx = nullptr;
  vb_.resource->Map(0, nullptr, reinterpret_cast<void **>(&vtx));
  if (!vtx)
    return;

  bool allZero = true;
  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    if (std::abs(vtx[i].texcoord.x) > 1e-8f ||
        std::abs(vtx[i].texcoord.y) > 1e-8f) {
      allZero = false;
      break;
    }
  }
  if (!allZero) {
    vb_.resource->Unmap(0, nullptr);
    return;
  }

  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    const float x = vtx[i].position.x;
    const float y = vtx[i].position.y;
    const float z = vtx[i].position.z;

    const float r = (std::max)(1e-6f, std::sqrt(x * x + y * y + z * z));
    const float theta = std::atan2(z, x); // [-pi, pi]
    const float phi =
        std::asin(std::clamp(y / r, -1.0f, 1.0f)); // [-pi/2, pi/2]

    float u = 0.5f + (theta / (2.0f * kPi));
    float v = 0.5f - (phi / (1.0f * kPi));

    if (u < 0.0f)
      u += 1.0f;
    if (u > 1.0f)
      u -= 1.0f;

    vtx[i].texcoord = {u, v};
  }

  vb_.resource->Unmap(0, nullptr);
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
}

void Model3D::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
}

void Model3D::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!vb_.resource)
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWvp_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  cmdList->DrawInstanced(vb_.vertexCount, 1, 0, 0);
}

// =========================
// 内部：OBJローダ（移植）
// =========================

bool Model3D::LoadObjToVertices_(const std::string &directoryPath,
                                 const std::string &filename,
                                 std::vector<VertexData> &outVertices,
                                 MaterialData &outMtl) {
  std::vector<Vector4> positions;
  std::vector<Vector3> normals;
  std::vector<Vector2> texcoords;

  std::ifstream file(directoryPath + "/" + filename);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::string id;
    std::istringstream s(line);
    s >> id;

    if (id == "v") {
      Vector4 p{};
      s >> p.x >> p.y >> p.z;
      p.x = -p.x;
      p.w = 1.0f;
      positions.push_back(p); // 左手系へ
    } else if (id == "vt") {
      Vector2 t{};
      s >> t.x >> t.y;
      t.y = 1.0f - t.y;
      texcoords.push_back(t);
    } else if (id == "vn") {
      Vector3 n{};
      s >> n.x >> n.y >> n.z;
      n.x = -n.x;
      normals.push_back(n);
    } else if (id == "f") {
      VertexData tri[3];
      for (int vi = 0; vi < 3; ++vi) {
        std::string vd;
        s >> vd;
        std::string idx[3] = {"", "", ""};
        size_t prev = 0, pos;
        int field = 0;
        while (field < 2 && (pos = vd.find('/', prev)) != std::string::npos) {
          idx[field++] = vd.substr(prev, pos - prev);
          prev = pos + 1;
        }
        if (prev < vd.size() && field < 3)
          idx[field++] = vd.substr(prev);

        int pi = std::stoi(idx[0]);
        int ti = (!idx[1].empty()) ? std::stoi(idx[1]) : 0;
        int ni = (!idx[2].empty()) ? std::stoi(idx[2]) : 0;

        Vector4 p = positions[pi - 1];
        Vector2 t = (ti > 0 && ti - 1 < (int)texcoords.size())
                        ? texcoords[ti - 1]
                        : Vector2{0, 0};
        Vector3 n = (ni > 0 && ni - 1 < (int)normals.size()) ? normals[ni - 1]
                                                             : Vector3{0, 0, 0};
        tri[vi] = {p, t, n};
      }
      // 面の向きを反転して格納（既存実装と同じ）
      outVertices.push_back(tri[2]);
      outVertices.push_back(tri[1]);
      outVertices.push_back(tri[0]);
    } else if (id == "mtllib") {
      std::string mtlName;
      s >> mtlName;
      outMtl = LoadMaterialTemplateFile_(directoryPath, mtlName);
    }
  }
  return true;
}

void Model3D::UploadVB_(const std::vector<VertexData> &vertices) {
  vb_.vertexCount = static_cast<uint32_t>(vertices.size());
  if (vb_.vertexCount == 0)
    return;

  const size_t sizeBytes = sizeof(VertexData) * vb_.vertexCount;
  vb_.resource = CreateBufferResource(device_, sizeBytes);

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = static_cast<UINT>(sizeBytes);
  vb_.view.StrideInBytes = sizeof(VertexData);
}

MaterialData
Model3D::LoadMaterialTemplateFile_(const std::string &directoryPath,
                                   const std::string &filename) {
  MaterialData materialData{};
  std::string line;
  std::ifstream file(directoryPath + "/" + filename);
  assert(file.is_open());

  while (std::getline(file, line)) {
    std::string identifier;
    std::istringstream s(line);
    s >> identifier;
    if (identifier == "map_Kd") {
      std::string textureFilename;
      s >> textureFilename;
      materialData.textureFilePath = directoryPath + "/" + textureFilename;
    }
  }
  return materialData;
}

// -------------------------------
// ライティング設定 4 引数版
// -------------------------------
Model3D &Model3D::SetLightingConfig(LightingMode mode,
                                    const std::array<float, 3> &color,
                                    const std::array<float, 3> &dir,
                                    float intensity) {
  initialLighting_.mode = mode;
  initialLighting_.color[0] = color[0];
  initialLighting_.color[1] = color[1];
  initialLighting_.color[2] = color[2];

  // ※必要ならここで正規化
  initialLighting_.dir[0] = dir[0];
  initialLighting_.dir[1] = dir[1];
  initialLighting_.dir[2] = dir[2];

  initialLighting_.intensity = intensity;

  ApplyLightingIfReady_();
  return *this;
}

void Model3D::ApplyLightingIfReady_() {
  if (cbMat_.mapped) {
    cbMat_.mapped->lightingMode = static_cast<int>(initialLighting_.mode);
  }
  if (cbLight_.mapped) {
    cbLight_.mapped->color = {initialLighting_.color[0],
                              initialLighting_.color[1],
                              initialLighting_.color[2], 1.0f};
    cbLight_.mapped->direction = {initialLighting_.dir[0],
                                  initialLighting_.dir[1],
                                  initialLighting_.dir[2]};
    cbLight_.mapped->intensity = initialLighting_.intensity;
  }
}
