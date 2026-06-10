#include "Skybox.h"
#include "../../Render/RenderContext.h"
#include "../../../Application/Game/Scene/Scene.h"
#include "../../Dx12/Dx12Core.h"
#include "../../Dx12/DeferredReleaseQueue/DeferredReleaseQueue.h"
#include "Math/Math.h"
#include "imgui/imgui.h"
#include <cassert>

using namespace RC;

Skybox::~Skybox() {
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

void Skybox::Initialize(ID3D12Device *device) {
  device_ = device;

  // ボックスメッシュ生成
  BuildBox_();
  UploadVB_();
  UploadIB_();

  // CB: WVP
  cbWvp_.resource = CreateBufferResource(device_.Get(),
                                         sizeof(TransformationMatrix),
                                         L"Skybox::cbWvp_");
  cbWvp_.resource->Map(0, nullptr,
                       reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();
  cbWvp_.mapped->worldInverseTranspose = MakeIdentity4x4();

  // CB: Material
  cbMat_.resource = CreateBufferResource(device_.Get(), sizeof(Material),
                                         L"Skybox::cbMat_");
  cbMat_.resource->Map(0, nullptr,
                       reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
  cbMat_.mapped->lightingMode = 0; // ライティング無効

  // CB: Light（ダミー）
  cbLight_.resource = CreateBufferResource(device_.Get(),
                                           sizeof(DirectionalLight),
                                           L"Skybox::cbLight_");
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 0.0f; // Skyboxはライティング不要
}

void Skybox::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));
}

void Skybox::Draw(ID3D12GraphicsCommandList *cmdList) {
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
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  cmdList->DrawIndexedInstanced(ib_.indexCount, 1, 0, 0, 0);
}

void Skybox::Draw(ID3D12GraphicsCommandList *cmdList, const Matrix4x4 &world) {
  if (!vb_.resource || !ib_.resource || !visible_ || !cbWvp_.mapped)
    return;

  auto &ctx = GetRenderContext();
  cbWvp_.mapped->World = world;
  Matrix4x4 vp = Multiply(ctx.View(), ctx.Proj());
  cbWvp_.mapped->WVP = Multiply(world, vp);
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));

  Draw(cmdList);
}

void Skybox::DrawImGui(const char *name) {
#if RC_ENABLE_IMGUI

  std::string label = name ? std::string(name) : std::string("Skybox");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;
  // ---- 表示ON/OFF ----
  bool vis = Visible();
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
    SetVisible(vis);

  if (vis) {
    // ---- Transform ----
    ImGui::TextUnformatted("Transform");
    ImGui::DragFloat3((std::string("スケール(x,y,z)##") + label).c_str(),
                      &transform_.scale.x, 1.0f, 1.0f, 10000.0f, "%.1f");
    ImGui::SliderAngle((std::string("回転Y##") + label).c_str(),
                       &transform_.rotation.y);
    if (ImGui::Button((std::string("Transformリセット##") + label).c_str())) {
      transform_.translation = {0, 0, 0};
      transform_.rotation = {0, 0, 0};
      transform_.scale = {100, 100, 100};
    }
    ImGui::Dummy(ImVec2(0, 6));
    // ---- Material（乗算カラー）----
    ImGui::TextUnformatted("Material");
    if (cbMat_.mapped) {
      ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                        &cbMat_.mapped->color.x, ImGuiColorEditFlags_Float);
    } else {
      ImGui::TextDisabled("Material CB not ready.");
    }
    ImGui::Dummy(ImVec2(0, 6));
  }

#endif
}

void Skybox::BuildBox_() {
  vertices_.clear();
  indices_.clear();

  // 箱の6面（内側を向く）
  // 各面に4頂点、合計24頂点
  // position のみ重要（texcoord/normal はダミー）

  struct Face {
    Vector4 v[4]; // 4頂点
  };

  // 内側を見るため、法線方向は内向き（インデックスで対応）
  Face faces[6] = {
      // 右面 (+X)
      {{{1, 1, 1, 1}, {1, 1, -1, 1}, {1, -1, 1, 1}, {1, -1, -1, 1}}},
      // 左面 (-X)
      {{{-1, 1, -1, 1}, {-1, 1, 1, 1}, {-1, -1, -1, 1}, {-1, -1, 1, 1}}},
      // 上面 (+Y)
      {{{-1, 1, -1, 1}, {1, 1, -1, 1}, {-1, 1, 1, 1}, {1, 1, 1, 1}}},
      // 下面 (-Y)
      {{{-1, -1, 1, 1}, {1, -1, 1, 1}, {-1, -1, -1, 1}, {1, -1, -1, 1}}},
      // 前面 (+Z)
      {{{-1, 1, 1, 1}, {1, 1, 1, 1}, {-1, -1, 1, 1}, {1, -1, 1, 1}}},
      // 後面 (-Z)
      {{{1, 1, -1, 1}, {-1, 1, -1, 1}, {1, -1, -1, 1}, {-1, -1, -1, 1}}},
  };

  for (int f = 0; f < 6; ++f) {
    uint16_t base = static_cast<uint16_t>(vertices_.size());

    for (int v = 0; v < 4; ++v) {
      VertexData vd{};
      vd.position = faces[f].v[v];
      vd.texcoord = {0.0f, 0.0f}; // Skyboxでは使わない
      vd.normal = {0.0f, 0.0f, 0.0f}; // Skyboxでは使わない
      vertices_.push_back(vd);
    }

    // 内側を向くインデックス（時計回り＝内側から見て反時計回り）
    // 三角形1: 0, 1, 2
    // 三角形2: 2, 1, 3
    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);

    indices_.push_back(base + 2);
    indices_.push_back(base + 1);
    indices_.push_back(base + 3);
  }
}

void Skybox::UploadVB_() {
  vb_.vertexCount = static_cast<uint32_t>(vertices_.size());
  if (vb_.vertexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(VertexData) * vb_.vertexCount);
  vb_.resource =
      CreateBufferResource(device_.Get(), sizeBytes, L"Skybox::vb_");

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices_.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeBytes;
  vb_.view.StrideInBytes = sizeof(VertexData);
}

void Skybox::UploadIB_() {
  ib_.indexCount = static_cast<uint32_t>(indices_.size());
  if (ib_.indexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(uint16_t) * ib_.indexCount);
  ib_.resource =
      CreateBufferResource(device_.Get(), sizeBytes, L"Skybox::ib_");
  assert(ib_.resource);

  uint16_t *mapped = nullptr;
  ib_.resource->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
  std::memcpy(mapped, indices_.data(), sizeBytes);
  ib_.resource->Unmap(0, nullptr);

  ib_.view.BufferLocation = ib_.resource->GetGPUVirtualAddress();
  ib_.view.Format = DXGI_FORMAT_R16_UINT;
  ib_.view.SizeInBytes = sizeBytes;
}
