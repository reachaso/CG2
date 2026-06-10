#include "Skydome.h"
#include "../../Render/RenderContext.h"
#include "../../../Application/Game/Scene/Scene.h"
#include "../../Dx12/Dx12Core.h"
#include "../../Dx12/DeferredReleaseQueue/DeferredReleaseQueue.h"
#include "Math/Math.h"
#include "imgui/imgui.h"
#include <cassert>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace RC;

Skydome::~Skydome() {
  if (auto ctx = GetRenderContext().Ctx()) {
    auto& dq = ctx->core->DeferredRelease();
    if (vb_.resource) dq.Enqueue(std::move(vb_.resource), UINT64_MAX);
    if (ib_.resource) dq.Enqueue(std::move(ib_.resource), UINT64_MAX);
    if (cbWvp_.resource) dq.Enqueue(std::move(cbWvp_.resource), UINT64_MAX);
    if (cbMat_.resource) dq.Enqueue(std::move(cbMat_.resource), UINT64_MAX);
    if (cbLight_.resource) dq.Enqueue(std::move(cbLight_.resource), UINT64_MAX);
  } else {
    vb_.resource.Reset();
    ib_.resource.Reset();
    cbWvp_.resource.Reset();
    cbMat_.resource.Reset();
    cbLight_.resource.Reset();
  }
}

void Skydome::Initialize(ID3D12Device *device, float radius, UINT sliceCount,
                        UINT stackCount) {
  device_ = device;

  // メッシュ生成
  BuildGeometry(radius, sliceCount, stackCount);
  UploadVB_();
  UploadIB_();

  // CB: WVP
  cbWvp_.resource = CreateBufferResource(device_.Get(), sizeof(TransformationMatrix), L"Skydome::cbWvp_");
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();
  cbWvp_.mapped->worldInverseTranspose = MakeIdentity4x4();

  // CB: Material
  cbMat_.resource = CreateBufferResource(device_.Get(), sizeof(Material), L"Skydome::cbMat_");
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
  cbMat_.mapped->lightingMode = 0; // 天球なので初期はライティング無効

  // CB: Light（天球ごとに持つが一応残す）
  cbLight_.resource = CreateBufferResource(device_.Get(), sizeof(DirectionalLight), L"Skydome::cbLight_");
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
  cbMat_.mapped->shininess = 32.0f;
}

void Skydome::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));
}

void Skydome::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!vb_.resource || !ib_.resource || !visible_)
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetIndexBuffer(&ib_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWvp_.resource->GetGPUVirtualAddress());
  
  if (textureSrv_.ptr != 0) {
      cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  }

  // Light CB（b1）: 外部ライトが指定されていればそちらを使う
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

  cmdList->DrawIndexedInstanced(ib_.indexCount, 1, 0, 0, 0);
}

void Skydome::Draw(ID3D12GraphicsCommandList *cmdList, const Matrix4x4 &world) {
  if (!vb_.resource || !ib_.resource || !visible_ || !cbWvp_.mapped)
    return;

  auto &ctx = GetRenderContext();
  cbWvp_.mapped->World = world;
  Matrix4x4 vp = Multiply(ctx.View(), ctx.Proj());
  cbWvp_.mapped->WVP = Multiply(world, vp);
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));


  Draw(cmdList);
}


void Skydome::DrawImGui(const char *name) {
#if RC_ENABLE_IMGUI

  std::string label = name ? std::string(name) : std::string("Skydome");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;
  // ---- 表示ON/OFF ----
  bool vis = Visible();
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
    SetVisible(vis);

  if (vis) {
    // ---- Transform ----
    ImGui::TextUnformatted("Transform");
    ImGui::DragFloat3((std::string("位置(x,y,z)##") + label).c_str(),
                      &transform_.translation.x, 0.1f, -4096.0f, 4096.0f,
                      "%.2f");
    ImGui::SliderAngle((std::string("回転X##") + label).c_str(),
                       &transform_.rotation.x);
    ImGui::SliderAngle((std::string("回転Y##") + label).c_str(),
                       &transform_.rotation.y);
    ImGui::SliderAngle((std::string("回転Z##") + label).c_str(),
                       &transform_.rotation.z);
    ImGui::DragFloat3((std::string("スケール(x,y,z)##") + label).c_str(),
                      &transform_.scale.x, 0.01f, 0.0001f, 1000.0f, "%.3f");
    if (ImGui::Button((std::string("Transformリセット##") + label).c_str())) {
      transform_.translation = {0, 0, 0};
      transform_.rotation = {0, 0, 0};
      transform_.scale = {1, 1, 1};
    }
    ImGui::Dummy(ImVec2(0, 6));
    // ---- Material（乗算カラー・UV行列）----
    ImGui::TextUnformatted("Material");
    if (cbMat_.mapped) {
      ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                        &cbMat_.mapped->color.x, ImGuiColorEditFlags_Float);
      // 簡易UVスケール＆平行移動（行列に焼く）
      static Vector3 uvS{1, 1, 1};
      static Vector3 uvT{0, 0, 0};
      bool updUV = false;
      updUV |= ImGui::DragFloat2((std::string("UV 平行移動##") + label).c_str(),
                                 &uvT.x, 0.005f);
      updUV |= ImGui::DragFloat2((std::string("UV スケール##") + label).c_str(),
                                 &uvS.x, 0.005f, 0.001f, 16.0f);
      if (updUV) {
        Matrix4x4 m = MakeIdentity4x4();
        m = Multiply(MakeScaleMatrix(uvS), m);
        m = Multiply(m, MakeTranslateMatrix(uvT));
        cbMat_.mapped->uvTransform = m;
      }
      if (ImGui::Button((std::string("UV行列リセット##") + label).c_str())) {
        cbMat_.mapped->uvTransform = MakeIdentity4x4();
        uvS = {1, 1, 1};
        uvT = {0, 0, 0};
      }

    } else {
      ImGui::TextDisabled("Material CB not ready.");
    }
    ImGui::Dummy(ImVec2(0, 6));
  }

#endif
}

void Skydome::BuildGeometry(float radius, UINT sliceCount, UINT stackCount) {
  vertices_.clear();
  indices_.clear();

  // 上極点
  vertices_.push_back({Vector4(0, +radius, 0, 1), Vector2(0.5f, 0.0f)});

  // 中間リング（緯度）
  for (UINT lat = 1; lat < stackCount; ++lat) {
    float phi = lat * (float(M_PI) / float(stackCount));
    float v = float(lat) / float(stackCount);

    for (UINT lon = 0; lon <= sliceCount; ++lon) {
      float theta = lon * (2.0f * float(M_PI) / float(sliceCount));
      float u = float(lon) / float(sliceCount);

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi);
      float z = radius * sinf(phi) * sinf(theta);

      vertices_.push_back({Vector4(x, y, z, 1), Vector2(u, v)});
    }
  }

  // 下極点
  vertices_.push_back({Vector4(0, -radius, 0, 1), Vector2(0.5f, 1.0f)});

  // 法線（位置の正規化 + 天球なので内側を向くように反転）
  for (auto &v : vertices_) {
    float len =
        std::sqrt(v.position.x * v.position.x + v.position.y * v.position.y +
                  v.position.z * v.position.z);
    if (len > 0.0f) {
      // 天球なので内向き
      v.normal.x = -v.position.x / len;
      v.normal.y = -v.position.y / len;
      v.normal.z = -v.position.z / len;
    } else {
      v.normal = {0.0f, -1.0f, 0.0f};
    }
  }

  // インデックス
  UINT ringVerts = sliceCount + 1;

  // 上極ファン（内向きなので順序を反転させる）
  for (UINT i = 1; i <= sliceCount; ++i) {
    indices_.push_back(0);
    indices_.push_back(i);
    indices_.push_back(i + 1);
  }

  // 中間帯のクアッド
  for (UINT i = 0; i < stackCount - 2; ++i) {
    for (UINT j = 0; j < sliceCount; ++j) {
      UINT a = 1 + i * ringVerts + j;
      UINT b = 1 + i * ringVerts + j + 1;
      UINT c = 1 + (i + 1) * ringVerts + j;
      UINT d = 1 + (i + 1) * ringVerts + j + 1;
      // 内向き
      indices_.push_back(a);
      indices_.push_back(d);
      indices_.push_back(b);
      indices_.push_back(a);
      indices_.push_back(c);
      indices_.push_back(d);
    }
  }

  // 下極ファン
  UINT southPoleIndex = UINT(vertices_.size() - 1);
  UINT baseIndex = southPoleIndex - ringVerts;
  for (UINT i = 0; i < sliceCount; ++i) {
    indices_.push_back(southPoleIndex);
    indices_.push_back(baseIndex + i + 1);
    indices_.push_back(baseIndex + i);
  }
}

void Skydome::UploadVB_() {
  vb_.vertexCount = static_cast<uint32_t>(vertices_.size());
  if (vb_.vertexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(VertexData) * vb_.vertexCount);
  vb_.resource = CreateBufferResource(device_.Get(), sizeBytes, L"Skydome::vb_");

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices_.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeBytes;
  vb_.view.StrideInBytes = sizeof(VertexData);
}

void Skydome::UploadIB_() {
  ib_.indexCount = static_cast<uint32_t>(indices_.size());
  if (ib_.indexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(uint16_t) * ib_.indexCount);
  ib_.resource = CreateBufferResource(device_.Get(), sizeBytes, L"Skydome::ib_");
  assert(ib_.resource);

  uint16_t *mapped = nullptr;
  ib_.resource->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
  std::memcpy(mapped, indices_.data(), sizeBytes);
  ib_.resource->Unmap(0, nullptr);

  ib_.view.BufferLocation = ib_.resource->GetGPUVirtualAddress();
  ib_.view.Format = DXGI_FORMAT_R16_UINT;
  ib_.view.SizeInBytes = sizeBytes;
}
