#include "GPUParticle.h"
#include "DeferredReleaseQueue/DeferredReleaseQueue.h"
#include "Dx12Core.h"
#include "PipelineManager.h"
#include "RenderCommon.h"
#include "RenderContext.h"
#include <Math/Math.h>
#include <function/function.h>
#include <format>

#include "Common/Log/Log.h"
#include "Common/EngineConfig.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

GPUParticle::~GPUParticle() { Finalize(); }

void GPUParticle::Initialize(SceneContext &ctx) {
  device_ = ctx.core->GetDevice();
  srvMgr_ = &ctx.core->SRVMan();
  deferredRelease_ = &ctx.core->DeferredRelease();

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
  // 2. FreeList バッファ作成（DEFAULT ヒープ、UAV 対応）
  // ==================
  {
    D3D12_HEAP_PROPERTIES heapProp{};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    // FreeList: uint32_t × kMaxParticles
    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sizeof(uint32_t) * kMaxParticles;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&freeListBuffer_));
    assert(SUCCEEDED(hr));
    freeListBuffer_->SetName(L"GPUParticle::freeListBuffer");

    // FreeListIndex: int32_t × 1
    bufDesc.Width = sizeof(int32_t);
    hr = device_->CreateCommittedResource(
        &heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&freeListIndexBuffer_));
    assert(SUCCEEDED(hr));
    freeListIndexBuffer_->SetName(L"GPUParticle::freeListIndexBuffer");
  }

  // ==================
  // 3. UAV / SRV ビュー作成
  // ==================
  uavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
  srvHandle_ = srvMgr_->CreateStructuredBuffer(
      particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

  freeListIndexUavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      freeListIndexBuffer_.Get(), 1, sizeof(int32_t));
  freeListUavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      freeListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

  // ==================
  // 4. PerView 定数バッファ作成（UPLOAD ヒープ）
  // ==================
  perViewCB_ = CreateBufferResource(device_.Get(),
                                    Align256(sizeof(GPUParticlePerView)),
                                    L"GPUParticle::perViewCB");
  perViewCB_->Map(0, nullptr, reinterpret_cast<void **>(&perViewMapped_));
  *perViewMapped_ = GPUParticlePerView{};

  // ==================
  // 5. 板ポリ VB 作成（6 頂点のクワッド）
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
  // 6. テクスチャ読み込み
  // ==================
  texHandle_ = RC::LoadTex("Resources/Particle/circle.png", true);

  // ==================
  // 7. ComputeShader パイプライン取得（実行は初回 Render に遅延）
  // ==================
  if (ctx.pipelineManager) {
    // 初期化 CS（タイプ共通）
    initCS_.Initialize(device_.Get(), ctx.pipelineManager, "init_particle_cs");
    if (initCS_.IsReady()) {
      needsCSInit_ = true;
    }

    // Default タイプ
    {
      auto &set = csSets_[static_cast<uint32_t>(ParticleType::Default)];
      set.emit.Initialize(device_.Get(), ctx.pipelineManager, "emit_particle_cs");
      set.update.Initialize(device_.Get(), ctx.pipelineManager, "update_particle_cs");
      set.ready = set.emit.IsReady() && set.update.IsReady();
    }

    // Explosion タイプ
    {
      auto &set = csSets_[static_cast<uint32_t>(ParticleType::Explosion)];
      set.emit.Initialize(device_.Get(), ctx.pipelineManager, "emit_explosion_cs");
      set.update.Initialize(device_.Get(), ctx.pipelineManager, "update_particle_cs"); // 更新は Default と同じ
      set.ready = set.emit.IsReady() && set.update.IsReady();
    }

    // Rain タイプ
    {
      auto &set = csSets_[static_cast<uint32_t>(ParticleType::Rain)];
      set.emit.Initialize(device_.Get(), ctx.pipelineManager, "emit_rain_cs");
      set.update.Initialize(device_.Get(), ctx.pipelineManager, "update_rain_cs");
      set.ready = set.emit.IsReady() && set.update.IsReady();
    }
  }

  // ==================
  // 8. PerFrame 定数バッファ作成（deltaTime 用）
  // ==================
  perFrameCB_ = CreateBufferResource(device_.Get(),
                                     Align256(sizeof(GPUParticlePerFrame)),
                                     L"GPUParticle::perFrameCB");
  perFrameCB_->Map(0, nullptr, reinterpret_cast<void **>(&perFrameMapped_));
  *perFrameMapped_ = GPUParticlePerFrame{};

  initialized_ = true;
  Log::Print(std::format("[GPUParticle] Initialized: {} particles (FreeList, {} types)",
                         kMaxParticles, kParticleTypeCount));
}

void GPUParticle::Finalize() {
  if (!initialized_)
    return;

  // 遅延解放キューが利用可能なら、GPU リソースをキューに委ねる
  // （GPU が参照中でも安全に解放される）
  const uint64_t currentFence = deferredRelease_ ? UINT64_MAX : 0;

  if (srvMgr_) {
    if (uavHandle_.IsValid()) {
      srvMgr_->Free(uavHandle_);
      uavHandle_ = {};
    }
    if (srvHandle_.IsValid()) {
      srvMgr_->Free(srvHandle_);
      srvHandle_ = {};
    }
    if (freeListUavHandle_.IsValid()) {
      srvMgr_->Free(freeListUavHandle_);
      freeListUavHandle_ = {};
    }
    if (freeListIndexUavHandle_.IsValid()) {
      srvMgr_->Free(freeListIndexUavHandle_);
      freeListIndexUavHandle_ = {};
    }
  }

  if (perViewCB_) {
    perViewCB_->Unmap(0, nullptr);
    perViewMapped_ = nullptr;
    if (deferredRelease_) {
      deferredRelease_->Enqueue(std::move(perViewCB_), currentFence);
    } else {
      perViewCB_.Reset();
    }
  }

  if (perFrameCB_) {
    perFrameCB_->Unmap(0, nullptr);
    perFrameMapped_ = nullptr;
    if (deferredRelease_) {
      deferredRelease_->Enqueue(std::move(perFrameCB_), currentFence);
    } else {
      perFrameCB_.Reset();
    }
  }

  // DEFAULT ヒープのバッファを遅延解放キューに移す
  if (deferredRelease_) {
    if (particleBuffer_) {
      deferredRelease_->Enqueue(std::move(particleBuffer_), currentFence);
    }
    if (freeListBuffer_) {
      deferredRelease_->Enqueue(std::move(freeListBuffer_), currentFence);
    }
    if (freeListIndexBuffer_) {
      deferredRelease_->Enqueue(std::move(freeListIndexBuffer_), currentFence);
    }
    if (vbResource_) {
      deferredRelease_->Enqueue(std::move(vbResource_), currentFence);
    }
  } else {
    particleBuffer_.Reset();
    freeListBuffer_.Reset();
    freeListIndexBuffer_.Reset();
    vbResource_.Reset();
  }

  device_.Reset();
  srvMgr_ = nullptr;
  deferredRelease_ = nullptr;
  initialized_ = false;
}

void GPUParticle::SetParticleType(ParticleType type) {
  if (type >= ParticleType::Count) return;
  if (type == currentType_) return;

  currentType_ = type;
  // タイプ変更時に FreeList を再初期化する
  needsCSInit_ = true;

  Log::Print(std::format("[GPUParticle] Type changed to: {}",
                         static_cast<int>(type)));
}

void GPUParticle::Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
                         float deltaTime) {
  if (!initialized_)
    return;

  // Billboard 行列の構築（ビュー行列の逆回転）
  RC::Matrix4x4 cameraWorld = Inverse(view);
  RC::Matrix4x4 billboard = MakeIdentity4x4();

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

  // PerFrame 更新
  if (perFrameMapped_) {
    perFrameMapped_->deltaTime = deltaTime;
  }
}

void GPUParticle::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (!initialized_ || !visible_)
    return;

  // 初回フレームで CS 初期化を実行（OnEnter 時はコマンドリスト未準備のため遅延）
  if (needsCSInit_) {
    initCS_.Bind(cl);
    initCS_.SetUAV(cl, 0, uavHandle_.gpu);
    initCS_.SetUAV(cl, 1, freeListIndexUavHandle_.gpu);
    initCS_.SetUAV(cl, 2, freeListUavHandle_.gpu);
    initCS_.DispatchDirect(cl, 1, 1, 1);
    ComputeShader::UAVBarrier(cl, particleBuffer_.Get());
    ComputeShader::UAVBarrier(cl, freeListBuffer_.Get());
    ComputeShader::UAVBarrier(cl, freeListIndexBuffer_.Get());
    needsCSInit_ = false;
    Log::Print("[GPUParticle] CS init executed (deferred, FreeList)");
  }

  // 現在のタイプの CS セットを取得
  const uint32_t typeIdx = static_cast<uint32_t>(currentType_);
  auto &csSet = csSets_[typeIdx];

  if (ctx.isPlaying() && csSet.ready) {
    // 毎フレーム EmitParticle CS を Dispatch（emitCount_ 個射出）
    if (perFrameCB_) {
      csSet.emit.Bind(cl);
      csSet.emit.SetUAV(cl, 0, uavHandle_.gpu);
      csSet.emit.SetUAV(cl, 1, freeListIndexUavHandle_.gpu);
      csSet.emit.SetUAV(cl, 2, freeListUavHandle_.gpu);
      csSet.emit.SetCBV(cl, 3, perFrameCB_->GetGPUVirtualAddress());
      csSet.emit.Dispatch(cl, emitCount_, 1024);
      ComputeShader::UAVBarrier(cl, particleBuffer_.Get());
      ComputeShader::UAVBarrier(cl, freeListIndexBuffer_.Get());
    }

    // 毎フレーム UpdateParticle CS を Dispatch
    if (perFrameCB_) {
      csSet.update.Bind(cl);
      csSet.update.SetUAV(cl, 0, uavHandle_.gpu);
      csSet.update.SetUAV(cl, 1, freeListIndexUavHandle_.gpu);
      csSet.update.SetUAV(cl, 2, freeListUavHandle_.gpu);
      csSet.update.SetCBV(cl, 3, perFrameCB_->GetGPUVirtualAddress());
      csSet.update.DispatchDirect(cl, 1, 1, 1);
      ComputeShader::UAVBarrier(cl, particleBuffer_.Get());
      ComputeShader::UAVBarrier(cl, freeListBuffer_.Get());
      ComputeShader::UAVBarrier(cl, freeListIndexBuffer_.Get());
    }
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

  // 描画コマンドをキューに積む（Execute3DCommands で Skybox 等と一緒にフラッシュ）
  // sortKey を大きくしてモデルの後（手前）に描画する
  constexpr uint64_t kParticleSortKey = UINT64_MAX - 1;
  auto *capturedPso = pso;
  auto vbView = vbView_;
  auto vertexCount = vertexCount_;
  auto perViewAddr = perViewCB_->GetGPUVirtualAddress();
  auto srvGpu = srvHandle_.gpu;
  auto blend = blendMode_;

  RC::RenderContext::GetInstance().PushCommand3D(
      kParticleSortKey,
      [capturedPso, vbView, vertexCount, perViewAddr, srvGpu, textureSrv,
       blend](ID3D12GraphicsCommandList *cmdList) {
        BlendMode prevBlend = RC::GetBlendMode();
        RC::SetBlendMode(blend);

        cmdList->SetGraphicsRootSignature(capturedPso->Root());
        cmdList->SetPipelineState(capturedPso->PSO());

        // IA 設定
        cmdList->IASetVertexBuffers(0, 1, &vbView);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // リソースバインド
        cmdList->SetGraphicsRootConstantBufferView(0, perViewAddr);
        cmdList->SetGraphicsRootDescriptorTable(1, srvGpu);
        cmdList->SetGraphicsRootDescriptorTable(2, textureSrv);

        // インスタンシング描画
        cmdList->DrawInstanced(vertexCount, kMaxParticles, 0, 0);

        RC::SetBlendMode(prevBlend);
      });
}

#if RC_ENABLE_IMGUI
void GPUParticle::DrawImGui() {
  if (ImGui::TreeNode("GPUParticle")) {
    ImGui::Checkbox("Visible", &visible_);
    int emit = static_cast<int>(emitCount_);
    if (ImGui::SliderInt("Emit Count", &emit, 0, 100)) {
      emitCount_ = static_cast<uint32_t>(emit);
    }

    // ParticleType 切り替え
    const char *typeNames[] = {"Default", "Explosion", "Rain"};
    int currentTypeInt = static_cast<int>(currentType_);
    if (ImGui::Combo("Particle Type", &currentTypeInt, typeNames,
                     IM_ARRAYSIZE(typeNames))) {
      SetParticleType(static_cast<ParticleType>(currentTypeInt));
    }

    // BlendMode
    const char *blendNames[] = {"None", "Normal", "Add", "Subtract",
                                "Multiply", "Screen", "Premultiplied"};
    int current = static_cast<int>(blendMode_);
    if (ImGui::Combo("BlendMode", &current, blendNames,
                     IM_ARRAYSIZE(blendNames))) {
      blendMode_ = static_cast<BlendMode>(current);
    }

    ImGui::TreePop();
  }
}
#else
void GPUParticle::DrawImGui() {}
#endif
