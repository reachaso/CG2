#include "Model3D.h"
#include "imgui/imgui.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numbers>

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
  cbMat_.mapped->lightingMode = 2; // デフォ：Half Lambert
  cbMat_.mapped->uvTransform = MakeIdentity4x4();

  // Light CB（各 Model が自前で持つ）
  cbLight_.resource = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
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

  // 頂点データへアクセス
  VertexData *vtx = nullptr;
  vb_.resource->Map(0, nullptr, reinterpret_cast<void **>(&vtx));
  if (!vtx)
    return;

  // 1) UVが本当に空(=全頂点が(0,0))かをチェック
  bool allZero = true;
  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    if (std::abs(vtx[i].texcoord.x) > 1e-8f ||
        std::abs(vtx[i].texcoord.y) > 1e-8f) {
      allZero = false;
      break;
    }
  }
  if (!allZero) {
    // 既にUVがあるなら何もしない
    vb_.resource->Unmap(0, nullptr);
    return;
  }

  // 2) 位置から球面UVを生成して焼き込む
  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    // 位置（左手系反転済み。x,zの反転はOBJパース側でやっています）
    const float x = vtx[i].position.x;
    const float y = vtx[i].position.y;
    const float z = vtx[i].position.z;

    // 球面座標（u: [-pi,pi], v: [-pi/2,pi/2]）
    const float r = (std::max)(1e-6f, std::sqrt(x * x + y * y + z * z));
    const float theta = std::atan2(z, x); // [-pi, pi]
    const float phi =
        std::asin(std::clamp(y / r, -1.0f, 1.0f)); // [-pi/2, pi/2]

    float u = 0.5f + (theta / (2.0f * kPi));
    float v = 0.5f - (phi / (1.0f * kPi));

    // 綺麗にループさせる
    if (u < 0.0f)
      u += 1.0f;
    if (u > 1.0f)
      u -= 1.0f;

    vtx[i].texcoord = {u, v};
  }

  vb_.resource->Unmap(0, nullptr);

  // 3) UV行列は単位でOK（ここでの見た目調整は不要）
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

  // VB
  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam 0: Material, 1: WVP, 2: SRV, 3: Light
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
      p.x = -p.x; // 左手系に合わせる既存挙動
      p.w = 1.0f;
      positions.push_back(p);
    } else if (id == "vt") {
      Vector2 t{};
      s >> t.x >> t.y;
      t.y = 1.0f - t.y; // 既存挙動
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
      // 既存実装と同様に向きを反転して格納
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
