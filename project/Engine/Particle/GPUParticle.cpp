#include "GPUParticle.h"
#include "Dx12Core.h"
#include "PipelineManager.h"
#include "RenderCommon.h"
#include <Math/Math.h>
#include <function/function.h>
#include <format>

#include "Common/Log/Log.h"
#include "Common/EngineConfig.h"

#if RC_ENABLE_IMGUI
#include <imgui/imgui.h>
#endif

GPUParticle::~GPUParticle() { Finalize(); }

void GPUParticle::Initialize(SceneContext &ctx) {
  device_ = ctx.core->GetDevice();
  srvMgr_ = &ctx.core->SRVMan();

  // ==================
  // 1. パーティクル用バッファ作成（DEFAULT ヒープ、UAV 対応）
  // ==================
  {
    D3D12_HEAP_PROPERTIES heapProp{};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sizeof(ParticleCS) * kMaxParticles;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&particleBuffer_));
    assert(SUCCEEDED(hr));
    particleBuffer_->SetName(L"GPUParticle::particleBuffer");
  }

  // ==================
  // 2. UAV / SRV ビュー作成
  // ==================
  uavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
  srvHandle_ = srvMgr_->CreateStructuredBuffer(
      particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

  // ==================
  // 3. PerView 定数バッファ作成（UPLOAD ヒープ）
  // ==================
  perViewCB_ = CreateBufferResource(device_.Get(),
                                    Align256(sizeof(GPUParticlePerView)),
                                    L"GPUParticle::perViewCB");
  perViewCB_->Map(0, nullptr, reinterpret_cast<void **>(&perViewMapped_));
  *perViewMapped_ = GPUParticlePerView{};

  // ==================
  // 4. 板ポリ VB 作成（6 頂点のクワッド）
  // ==================
  {
    struct SimpleVertex {
      RC::Vector4 position;
      RC::Vector2 texcoord;
      RC::Vector3 normal;
    };

    SimpleVertex quad[6] = {
        {{-1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    };
    vertexCount_ = 6;

    // VertexData にはスキニング用の boneIndices/boneWeights があるが、
    // InputLayout は Particle 用（POSITION, TEXCOORD, NORMAL）なので
    // VertexData のサイズで作成し、余分なフィールドは 0 クリアされる
    vbResource_ = CreateBufferResource(device_.Get(),
                                       sizeof(VertexData) * vertexCount_,
                                       L"GPUParticle::vbResource");

    VertexData *vbMapped = nullptr;
    vbResource_->Map(0, nullptr, reinterpret_cast<void **>(&vbMapped));
    for (uint32_t i = 0; i < vertexCount_; ++i) {
      vbMapped[i] = VertexData{};
      vbMapped[i].position = quad[i].position;
      vbMapped[i].texcoord = quad[i].texcoord;
      vbMapped[i].normal = quad[i].normal;
    }
    vbResource_->Unmap(0, nullptr);

    vbView_.BufferLocation = vbResource_->GetGPUVirtualAddress();
    vbView_.StrideInBytes = sizeof(VertexData);
    vbView_.SizeInBytes = sizeof(VertexData) * vertexCount_;
  }

  // ==================
  // 5. テクスチャ読み込み
  // ==================
  texHandle_ = RC::LoadTex("Resources/Particle/circle.png", true);

  // ==================
  // 6. ComputeShader パイプライン取得（実行は初回 Render に遅延）
  // ==================
  if (ctx.pipelineManager) {
    initCS_.Initialize(device_.Get(), ctx.pipelineManager, "init_particle_cs");
    if (initCS_.IsReady()) {
      needsCSInit_ = true;
    }
  }

  initialized_ = true;
  Log::Print(std::format("[GPUParticle] Initialized: {} particles", kMaxParticles));
}

void GPUParticle::Finalize() {
  if (!initialized_)
    return;

  if (srvMgr_) {
    if (uavHandle_.IsValid()) {
      srvMgr_->Free(uavHandle_);
      uavHandle_ = {};
    }
    if (srvHandle_.IsValid()) {
      srvMgr_->Free(srvHandle_);
      srvHandle_ = {};
    }
  }

  if (perViewCB_) {
    perViewCB_->Unmap(0, nullptr);
    perViewMapped_ = nullptr;
    perViewCB_.Reset();
  }

  particleBuffer_.Reset();
  vbResource_.Reset();
  device_.Reset();
  srvMgr_ = nullptr;
  initialized_ = false;
}

void GPUParticle::Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj) {
  if (!perViewMapped_)
    return;

  using namespace RC;

  // ビルボード行列: カメラのワールド行列から回転成分を抽出
  Matrix4x4 cameraWorld = Inverse(view);
  Matrix4x4 billboard = MakeIdentity4x4();

  // 右方向
  billboard.m[0][0] = cameraWorld.m[0][0];
  billboard.m[0][1] = cameraWorld.m[0][1];
  billboard.m[0][2] = cameraWorld.m[0][2];

  // 上方向
  billboard.m[1][0] = cameraWorld.m[1][0];
  billboard.m[1][1] = cameraWorld.m[1][1];
  billboard.m[1][2] = cameraWorld.m[1][2];

  // 前方向（反転）
  billboard.m[2][0] = -cameraWorld.m[2][0];
  billboard.m[2][1] = -cameraWorld.m[2][1];
  billboard.m[2][2] = -cameraWorld.m[2][2];

  billboard.m[3][0] = 0.0f;
  billboard.m[3][1] = 0.0f;
  billboard.m[3][2] = 0.0f;
  billboard.m[3][3] = 1.0f;

  // viewProjection = view * proj
  perViewMapped_->viewProjection = Multiply(view, proj);
  perViewMapped_->billboardMatrix = billboard;
}

void GPUParticle::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (!initialized_ || !visible_)
    return;

  // 初回フレームで CS 初期化を実行（OnEnter 時はコマンドリスト未準備のため遅延）
  if (needsCSInit_) {
    initCS_.Bind(cl);
    initCS_.SetUAV(cl, 0, uavHandle_.gpu);
    initCS_.DispatchDirect(cl, 1, 1, 1);
    ComputeShader::UAVBarrier(cl, particleBuffer_.Get());
    needsCSInit_ = false;
    Log::Print("[GPUParticle] CS init executed (deferred)");
  }

  // テクスチャ SRV（非同期ロード対応）
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv = RC::GetSrv(texHandle_);
  if (textureSrv.ptr == 0)
    return;

  if (!vbResource_ || !srvHandle_.IsValid() || !perViewCB_)
    return;

  // パイプライン取得
  GraphicsPipeline *pso = nullptr;
  if (ctx.pipelineManager) {
    pso = ctx.pipelineManager->Get(
        PipelineManager::MakeKey("gpu_particle", blendMode_));
    if (!pso && blendMode_ != kBlendModeNormal) {
      pso = ctx.pipelineManager->Get(
          PipelineManager::MakeKey("gpu_particle", kBlendModeNormal));
    }
  }
  if (!pso)
    return;

  BlendMode prevBlend = RC::GetBlendMode();
  RC::SetBlendMode(blendMode_);

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());

  // IA 設定
  cl->IASetVertexBuffers(0, 1, &vbView_);
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // リソースバインド
  // 0: CBV b0 (VS) PerView
  cl->SetGraphicsRootConstantBufferView(0, perViewCB_->GetGPUVirtualAddress());
  // 1: SRV table t0 (VS) Particles
  cl->SetGraphicsRootDescriptorTable(1, srvHandle_.gpu);
  // 2: SRV table t0 (PS) Texture
  cl->SetGraphicsRootDescriptorTable(2, textureSrv);

  // インスタンシング描画
  cl->DrawInstanced(vertexCount_, kMaxParticles, 0, 0);

  RC::SetBlendMode(prevBlend);
}

#if RC_ENABLE_IMGUI
void GPUParticle::DrawImGui() {
  if (ImGui::TreeNode("GPUParticle")) {
    ImGui::Checkbox("Visible", &visible_);
    ImGui::Text("Max Particles: %u", kMaxParticles);
    ImGui::Text("Buffer Size: %u bytes",
                static_cast<uint32_t>(sizeof(ParticleCS) * kMaxParticles));

    {
      static const char *kBlendNames[] = {
          "None", "Normal", "Add", "Subtract", "Multiply", "Screen",
      };
      int current = static_cast<int>(blendMode_);
      if (ImGui::Combo("Blend Mode##gpu", &current, kBlendNames,
                       IM_ARRAYSIZE(kBlendNames))) {
        blendMode_ = static_cast<BlendMode>(current);
      }
    }

    ImGui::TreePop();
  }
}
#else
void GPUParticle::DrawImGui() {}
#endif
