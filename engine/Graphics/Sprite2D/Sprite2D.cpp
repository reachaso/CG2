#include "Sprite2D.h"
#include <cassert>

// ---- 内部ユーティリティ ----
static void WriteQuadVertices(ID3D12Resource *vb) {
  VertexData *v = nullptr;
  vb->Map(0, nullptr, reinterpret_cast<void **>(&v));
  // 左下, 左上, 右下, 右上（UVは上0 下1）
  v[0].position = {0.0f, 1.0f, 0.0f, 1.0f};
  v[0].texcoord = {0.0f, 1.0f};
  v[0].normal = {0, 0, -1};
  v[1].position = {0.0f, 0.0f, 0.0f, 1.0f};
  v[1].texcoord = {0.0f, 0.0f};
  v[1].normal = {0, 0, -1};
  v[2].position = {1.0f, 1.0f, 0.0f, 1.0f};
  v[2].texcoord = {1.0f, 1.0f};
  v[2].normal = {0, 0, -1};
  v[3].position = {1.0f, 0.0f, 0.0f, 1.0f};
  v[3].texcoord = {1.0f, 0.0f};
  v[3].normal = {0, 0, -1};
  vb->Unmap(0, nullptr);
}

static void WriteQuadIndices(ID3D12Resource *ib) {
  uint32_t *idx = nullptr;
  ib->Map(0, nullptr, reinterpret_cast<void **>(&idx));
  idx[0] = 0;
  idx[1] = 1;
  idx[2] = 2;
  idx[3] = 1;
  idx[4] = 3;
  idx[5] = 2; // 2トライアングル
  ib->Unmap(0, nullptr);
}

// ---- ライフサイクル ----
Sprite2D::~Sprite2D() { Release(); }

void Sprite2D::Release() {
  if (vb_.res) {
    vb_.res->Release();
    vb_.res = nullptr;
  }
  if (ib_.res) {
    ib_.res->Release();
    ib_.res = nullptr;
  }
  if (cbWVP_.res) {
    cbWVP_.res->Release();
    cbWVP_.res = nullptr;
    cbWVP_.map = nullptr;
  }
  if (cbMat_.res) {
    cbMat_.res->Release();
    cbMat_.res = nullptr;
    cbMat_.map = nullptr;
  }
}

// ---- 初期化 ----
void Sprite2D::Initialize(ID3D12Device *device, float screenWidth,
                          float screenHeight) {
  Release();
  device_ = device;
  screenW_ = screenWidth;
  screenH_ = screenHeight;

  // CB: WVP
  cbWVP_.res = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWVP_.res->Map(0, nullptr, reinterpret_cast<void **>(&cbWVP_.map));
  cbWVP_.map->World = MakeIdentity4x4();
  cbWVP_.map->WVP = MakeIdentity4x4();

  // CB: Material（lightingMode=0, color=白, uvTransform=I）
  cbMat_.res = CreateBufferResource(device_, sizeof(Material));
  cbMat_.res->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.map));
  cbMat_.map->color = {1, 1, 1, 1};
  cbMat_.map->lightingMode = 0; // スプライトは既定でライティング無し
  cbMat_.map->uvTransform = MakeIdentity4x4();

  // VB (4頂点)
  vb_.res = CreateBufferResource(device_, sizeof(VertexData) * 4);
  WriteQuadVertices(vb_.res);
  vb_.view.BufferLocation = vb_.res->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeof(VertexData) * 4;
  vb_.view.StrideInBytes = sizeof(VertexData);

  // IB (6 index)
  ib_.res = CreateBufferResource(device_, sizeof(uint32_t) * 6);
  WriteQuadIndices(ib_.res);
  ib_.view.BufferLocation = ib_.res->GetGPUVirtualAddress();
  ib_.view.SizeInBytes = sizeof(uint32_t) * 6;
  ib_.view.Format = DXGI_FORMAT_R32_UINT;

  // ビュー/プロジェクション（左上基準の直交）
  view_ = MakeIdentity4x4();
  proj_ = MakeOrthographicMatrix(0.0f, 0.0f, screenW_, screenH_, 0.0f, 100.0f);

  // 既定サイズ
  SetSize(transform_.scale.x, transform_.scale.y); // 初期scaleを反映
}

void Sprite2D::SetScreenSize(float w, float h) {
  screenW_ = w;
  screenH_ = h;
  proj_ = MakeOrthographicMatrix(0.0f, 0.0f, screenW_, screenH_, 0.0f, 100.0f);
}

void Sprite2D::SetSize(float w, float h) {
  transform_.scale.x = w;
  transform_.scale.y = h;
  transform_.scale.z = 1.0f;
}

// ---- 毎フレーム ----
void Sprite2D::Update() {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWVP_.map->World = world;
  cbWVP_.map->WVP = Multiply(world, Multiply(view_, proj_));
}

// ---- 描画 ----
void Sprite2D::Draw(ID3D12GraphicsCommandList *cmdList) const {
  if (!visible_)
    return;
  assert(cmdList);

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetIndexBuffer(&ib_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0=Material, 1=WVP, 2=SRV
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.res->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWVP_.res->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, srv_);

  cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite2D::DrawImGui(const char *name) {
  std::string label = name ? std::string(name) : std::string("Sprite2D");
  if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    // 表示 ON/OFF（Sound.cpp 流儀のID分離）
    bool vis = Visible();
    if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
      SetVisible(vis);

    // 位置・回転・サイズ
    ImGui::DragFloat2((std::string("位置(x,y)##") + label).c_str(),
                      &transform_.translation.x, 1.0f, -4096.0f, 4096.0f,
                      "%.1f");
    ImGui::SliderAngle((std::string("回転Z##") + label).c_str(),
                       &transform_.rotation.z);
    ImGui::DragFloat2((std::string("サイズ(w,h)##") + label).c_str(),
                      &transform_.scale.x, 1.0f, 0.0f, 8192.0f, "%.1f");

    // 乗算カラー
    ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                      &cbMat_.map->color.x, ImGuiColorEditFlags_Float);

    // 簡易UVオフセット＆スケール（行列に反映）
    static Vector3 uvS{1, 1, 1};
    static Vector3 uvT{0, 0, 0};
    bool updateUV = false;
    updateUV |= ImGui::DragFloat2((std::string("UV平行移動##") + label).c_str(),
                                  &uvT.x, 0.005f);
    updateUV |= ImGui::DragFloat2((std::string("UVスケール##") + label).c_str(),
                                  &uvS.x, 0.005f, 0.001f, 16.0f);
    if (updateUV) {
      Matrix4x4 m = MakeIdentity4x4();
      m = Multiply(MakeScaleMatrix(uvS), m);
      m = Multiply(m, MakeTranslateMatrix(uvT));
      cbMat_.map->uvTransform = m;
    }

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
  }
}
