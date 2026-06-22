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

#include <fstream>
#include <nlohmann/json.hpp>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

GPUParticle::~GPUParticle() { Finalize(); }

void GPUParticle::Initialize(SceneContext &ctx) {
  device_ = ctx.core->GetDevice();
  srvMgr_ = &ctx.core->SRVMan();
  deferredRelease_ = &ctx.core->DeferredRelease();

  // ==================
  // 1〜3. パーティクル用バッファ・FreeList・UAV/SRV 作成
  // ==================
  rebuildBuffers_();

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
  perFrameMapped_->maxParticles = maxParticles_;
  perFrameMapped_->minLifeTime = minLifeTime_;
  perFrameMapped_->maxLifeTime = maxLifeTime_;
  perFrameMapped_->minScale = minScale_;
  perFrameMapped_->maxScale = maxScale_;
  perFrameMapped_->gravity = gravity_;
  perFrameMapped_->emitterShape = static_cast<uint32_t>(emitterShape_);
  perFrameMapped_->baseVelocity = baseVelocity_;
  perFrameMapped_->velocityVariance = velocityVariance_;
  perFrameMapped_->shapeRadius = shapeRadius_;
  perFrameMapped_->coneAngle = coneAngle_;
  perFrameMapped_->shapePad = {0.0f, 0.0f};
  perFrameMapped_->startColor = startColor_;
  perFrameMapped_->endColor = endColor_;
  perFrameMapped_->emitterPosition = emitterPosition_;
  perFrameMapped_->emitCount = emitCount_;
  perFrameMapped_->shapeBoxSize = shapeBoxSize_;
  perFrameMapped_->shapeBoxPad = 0.0f;

  initialized_ = true;
  Log::Print(std::format("[GPUParticle] Initialized: {} particles (FreeList, {} types)",
                         maxParticles_, kParticleTypeCount));
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
  if (!visible_ || !initialized_)
    return;

  // 定数バッファの更新（エミッタパラメータも含む）
  perFrameMapped_->deltaTime = deltaTime;
  perFrameMapped_->maxParticles = maxParticles_;
  perFrameMapped_->minLifeTime = minLifeTime_;
  perFrameMapped_->maxLifeTime = maxLifeTime_;
  perFrameMapped_->minScale = minScale_;
  perFrameMapped_->maxScale = maxScale_;
  perFrameMapped_->gravity = gravity_;
  perFrameMapped_->emitterShape = static_cast<uint32_t>(emitterShape_);
  perFrameMapped_->baseVelocity = baseVelocity_;
  perFrameMapped_->velocityVariance = velocityVariance_;
  perFrameMapped_->shapeRadius = shapeRadius_;
  perFrameMapped_->coneAngle = coneAngle_;
  perFrameMapped_->shapePad = {0.0f, 0.0f};
  perFrameMapped_->startColor = startColor_;
  perFrameMapped_->endColor = endColor_;
  perFrameMapped_->emitterPosition = emitterPosition_;
  perFrameMapped_->emitCount = emitCount_;
  perFrameMapped_->shapeBoxSize = shapeBoxSize_;
  perFrameMapped_->shapeBoxPad = 0.0f;
}

void GPUParticle::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (!initialized_ || !visible_)
    return;

  // 初回フレームで CS 初期化を実行（OnEnter 時はコマンドリスト未準備のため遅延）
  if (needsCSInit_ && initCS_.IsReady()) {
    initCS_.Bind(cl);
    initCS_.SetUAV(cl, 0, uavHandle_.gpu);
    initCS_.SetUAV(cl, 1, freeListIndexUavHandle_.gpu);
    initCS_.SetUAV(cl, 2, freeListUavHandle_.gpu);
    if (perFrameCB_) {
      initCS_.SetCBV(cl, 3, perFrameCB_->GetGPUVirtualAddress());
    }
    initCS_.Dispatch(cl, maxParticles_);
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
      csSet.update.Dispatch(cl, maxParticles_);
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
    std::string prefix = isPreview_ ? "gpu_particle_nodepth" : "gpu_particle";
    pso = ctx.pipelineManager->Get(
        PipelineManager::MakeKey(prefix, blendMode_));
    if (!pso && blendMode_ != kBlendModeNormal) {
      pso = ctx.pipelineManager->Get(
          PipelineManager::MakeKey(prefix, kBlendModeNormal));
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
  auto maxParticles = maxParticles_;

  auto pBuffer = particleBuffer_.Get();
  auto drawFunc = [capturedPso, vbView, vertexCount, maxParticles, perViewAddr, srvGpu, textureSrv,
                   blend, pBuffer](ID3D12GraphicsCommandList *cmdList) {
    BlendMode prevBlend = RC::GetBlendMode();
    RC::SetBlendMode(blend);

    // UAV -> SRV
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = pBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

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
    cmdList->DrawInstanced(vertexCount, maxParticles, 0, 0);

    // SRV -> UAV (次回 CS のため)
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    cmdList->ResourceBarrier(1, &barrier);

    RC::SetBlendMode(prevBlend);
  };

  if (isPreview_) {
    // プレビューモードの場合は直接描画（現在のターゲットに書き込む）
    drawFunc(cl);
  } else {
    // 通常の描画コマンドをキューに積む（Execute3DCommands で Skybox 等と一緒にフラッシュ）
    RC::RenderContext::GetInstance().PushCommand3D(kParticleSortKey, drawFunc);
  }
}

#if RC_ENABLE_IMGUI
void GPUParticle::DrawImGui() {
  if (ImGui::TreeNode("GPUParticle")) {
    ImGui::Checkbox("Visible", &visible_);
  
    int maxP = static_cast<int>(maxParticles_);
    if (ImGui::SliderInt("Max Particles", &maxP, 256, 16384)) {
      SetMaxParticles(static_cast<uint32_t>(maxP));
    }

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

    ImGui::Separator();
    ImGui::Text("--- Emitter Parameters ---");

    // エミッタ形状
    const char *shapeNames[] = {"Point", "Sphere", "Box", "Cone"};
    int shapeInt = static_cast<int>(emitterShape_);
    if (ImGui::Combo("Emitter Shape", &shapeInt, shapeNames, IM_ARRAYSIZE(shapeNames))) {
      emitterShape_ = static_cast<EmitterShape>(shapeInt);
    }

    // 形状別パラメータ
    if (emitterShape_ == EmitterShape::Sphere || emitterShape_ == EmitterShape::Cone) {
      ImGui::DragFloat("Shape Radius", &shapeRadius_, 0.1f, 0.0f, 50.0f);
    }
    if (emitterShape_ == EmitterShape::Cone) {
      float angleDeg = coneAngle_ * 180.0f / 3.14159265f;
      if (ImGui::DragFloat("Cone Angle (deg)", &angleDeg, 1.0f, 0.0f, 90.0f)) {
        coneAngle_ = angleDeg * 3.14159265f / 180.0f;
      }
    }
    if (emitterShape_ == EmitterShape::Box) {
      ImGui::DragFloat3("Box Size", &shapeBoxSize_.x, 0.1f, 0.0f, 50.0f);
    }

    // 位置
    ImGui::DragFloat3("Emitter Position", &emitterPosition_.x, 0.1f);

    // ライフタイム
    ImGui::DragFloat("Min Lifetime", &minLifeTime_, 0.1f, 0.1f, 30.0f);
    ImGui::DragFloat("Max Lifetime", &maxLifeTime_, 0.1f, 0.1f, 30.0f);

    // スケール
    ImGui::DragFloat("Min Scale", &minScale_, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat("Max Scale", &maxScale_, 0.01f, 0.01f, 5.0f);

    // 速度
    ImGui::DragFloat3("Base Velocity", &baseVelocity_.x, 0.001f);
    ImGui::DragFloat("Velocity Variance", &velocityVariance_, 0.001f, 0.0f, 1.0f);

    // 重力
    ImGui::DragFloat("Gravity", &gravity_, 0.01f, 0.0f, 20.0f);

    // 色
    ImGui::ColorEdit4("Start Color", &startColor_.x);
    ImGui::ColorEdit4("End Color", &endColor_.x);

    // テクスチャ
    ImGui::Text("Texture: %s", texturePath_.c_str());

    ImGui::TreePop();
  }
}
#else
void GPUParticle::DrawImGui() {}
#endif

void GPUParticle::SetTexture(const std::string& path) {
  texturePath_ = path;
  texHandle_ = RC::LoadTex(path, true);
  Log::Print(std::format("[GPUParticle] Texture changed to: {}", path));
}

void GPUParticle::SaveToJson(const std::string& filepath) const {
  nlohmann::json j;
  j["maxParticles"] = maxParticles_;
  j["emitCount"] = emitCount_;
  j["particleType"] = static_cast<int>(currentType_);
  j["blendMode"] = static_cast<int>(blendMode_);
  j["texturePath"] = texturePath_;

  j["minLifeTime"] = minLifeTime_;
  j["maxLifeTime"] = maxLifeTime_;
  j["minScale"] = minScale_;
  j["maxScale"] = maxScale_;
  j["gravity"] = gravity_;

  j["baseVelocity"] = { baseVelocity_.x, baseVelocity_.y, baseVelocity_.z };
  j["velocityVariance"] = velocityVariance_;

  j["emitterShape"] = static_cast<int>(emitterShape_);
  j["shapeRadius"] = shapeRadius_;
  j["coneAngle"] = coneAngle_;
  j["shapeBoxSize"] = { shapeBoxSize_.x, shapeBoxSize_.y, shapeBoxSize_.z };

  j["startColor"] = { startColor_.x, startColor_.y, startColor_.z, startColor_.w };
  j["endColor"] = { endColor_.x, endColor_.y, endColor_.z, endColor_.w };
  j["emitterPosition"] = { emitterPosition_.x, emitterPosition_.y, emitterPosition_.z };

  std::ofstream ofs(filepath);
  if (ofs) {
    ofs << j.dump(4);
    Log::Print(std::format("[GPUParticle] Saved to: {}", filepath));
  } else {
    Log::Print(std::format("[GPUParticle] Failed to save: {}", filepath));
  }
}

void GPUParticle::LoadFromJson(const std::string& filepath) {
  std::ifstream ifs(filepath);
  if (!ifs) {
    Log::Print(std::format("[GPUParticle] Failed to load: {}", filepath));
    return;
  }

  try {
    nlohmann::json j;
    ifs >> j;

    if (j.contains("maxParticles")) SetMaxParticles(j["maxParticles"].get<uint32_t>());
    if (j.contains("emitCount")) emitCount_ = j["emitCount"].get<uint32_t>();
    if (j.contains("particleType")) SetParticleType(static_cast<ParticleType>(j["particleType"].get<int>()));
    if (j.contains("blendMode")) blendMode_ = static_cast<BlendMode>(j["blendMode"].get<int>());
    if (j.contains("texturePath")) SetTexture(j["texturePath"].get<std::string>());

    if (j.contains("minLifeTime")) minLifeTime_ = j["minLifeTime"].get<float>();
    if (j.contains("maxLifeTime")) maxLifeTime_ = j["maxLifeTime"].get<float>();
    if (j.contains("minScale")) minScale_ = j["minScale"].get<float>();
    if (j.contains("maxScale")) maxScale_ = j["maxScale"].get<float>();
    if (j.contains("gravity")) gravity_ = j["gravity"].get<float>();

    if (j.contains("baseVelocity") && j["baseVelocity"].is_array() && j["baseVelocity"].size() == 3) {
      baseVelocity_ = { j["baseVelocity"][0].get<float>(), j["baseVelocity"][1].get<float>(), j["baseVelocity"][2].get<float>() };
    }
    if (j.contains("velocityVariance")) velocityVariance_ = j["velocityVariance"].get<float>();

    if (j.contains("emitterShape")) emitterShape_ = static_cast<EmitterShape>(j["emitterShape"].get<int>());
    if (j.contains("shapeRadius")) shapeRadius_ = j["shapeRadius"].get<float>();
    if (j.contains("coneAngle")) coneAngle_ = j["coneAngle"].get<float>();
    if (j.contains("shapeBoxSize") && j["shapeBoxSize"].is_array() && j["shapeBoxSize"].size() == 3) {
      shapeBoxSize_ = { j["shapeBoxSize"][0].get<float>(), j["shapeBoxSize"][1].get<float>(), j["shapeBoxSize"][2].get<float>() };
    }

    if (j.contains("startColor") && j["startColor"].is_array() && j["startColor"].size() == 4) {
      startColor_ = { j["startColor"][0].get<float>(), j["startColor"][1].get<float>(), j["startColor"][2].get<float>(), j["startColor"][3].get<float>() };
    }
    if (j.contains("endColor") && j["endColor"].is_array() && j["endColor"].size() == 4) {
      endColor_ = { j["endColor"][0].get<float>(), j["endColor"][1].get<float>(), j["endColor"][2].get<float>(), j["endColor"][3].get<float>() };
    }
    if (j.contains("emitterPosition") && j["emitterPosition"].is_array() && j["emitterPosition"].size() == 3) {
      emitterPosition_ = { j["emitterPosition"][0].get<float>(), j["emitterPosition"][1].get<float>(), j["emitterPosition"][2].get<float>() };
    }

    Log::Print(std::format("[GPUParticle] Loaded from: {}", filepath));
  } catch (const std::exception& e) {
    Log::Print(std::format("[GPUParticle] Error loading JSON: {}", e.what()));
  }
}

void GPUParticle::SetMaxParticles(uint32_t maxCount) {
  if (maxParticles_ == maxCount) return;
  maxParticles_ = maxCount;
  
  if (initialized_) {
    rebuildBuffers_();
  }
}

void GPUParticle::rebuildBuffers_() {
  if (!device_ || !srvMgr_ || !deferredRelease_) return;

  // 1. 既存リソースの遅延解放
  if (particleBuffer_) {
    deferredRelease_->Enqueue(particleBuffer_, UINT64_MAX);
    particleBuffer_ = nullptr;
  }
  if (freeListBuffer_) {
    deferredRelease_->Enqueue(freeListBuffer_, UINT64_MAX);
    freeListBuffer_ = nullptr;
  }
  if (freeListIndexBuffer_) {
    deferredRelease_->Enqueue(freeListIndexBuffer_, UINT64_MAX);
    freeListIndexBuffer_ = nullptr;
  }

  // 既存ハンドルの即時解放 (GPU側はバッファ自体がFence待ちになるため安全)
  if (uavHandle_.IsValid()) { srvMgr_->Free(uavHandle_); uavHandle_ = {}; }
  if (srvHandle_.IsValid()) { srvMgr_->Free(srvHandle_); srvHandle_ = {}; }
  if (freeListUavHandle_.IsValid()) { srvMgr_->Free(freeListUavHandle_); freeListUavHandle_ = {}; }
  if (freeListIndexUavHandle_.IsValid()) { srvMgr_->Free(freeListIndexUavHandle_); freeListIndexUavHandle_ = {}; }

  // 2. バッファ再作成
  D3D12_HEAP_PROPERTIES heapProp{};
  heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC bufDesc{};
  bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufDesc.Height = 1;
  bufDesc.DepthOrArraySize = 1;
  bufDesc.MipLevels = 1;
  bufDesc.SampleDesc.Count = 1;
  bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  // particleBuffer_
  bufDesc.Width = sizeof(ParticleCS) * maxParticles_;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&particleBuffer_));
  assert(SUCCEEDED(hr));
  particleBuffer_->SetName(L"GPUParticle::particleBuffer");

  // freeListBuffer_
  bufDesc.Width = sizeof(uint32_t) * maxParticles_;
  hr = device_->CreateCommittedResource(
      &heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&freeListBuffer_));
  assert(SUCCEEDED(hr));
  freeListBuffer_->SetName(L"GPUParticle::freeListBuffer");

  // freeListIndexBuffer_
  bufDesc.Width = sizeof(int32_t);
  hr = device_->CreateCommittedResource(
      &heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&freeListIndexBuffer_));
  assert(SUCCEEDED(hr));
  freeListIndexBuffer_->SetName(L"GPUParticle::freeListIndexBuffer");

  // 3. SRV / UAV 再作成
  uavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      particleBuffer_.Get(), maxParticles_, sizeof(ParticleCS));
  srvHandle_ = srvMgr_->CreateStructuredBuffer(
      particleBuffer_.Get(), maxParticles_, sizeof(ParticleCS));
  freeListIndexUavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      freeListIndexBuffer_.Get(), 1, sizeof(int32_t));
  freeListUavHandle_ = srvMgr_->CreateStructuredBufferUAV(
      freeListBuffer_.Get(), maxParticles_, sizeof(uint32_t));

  // 次回の Update() の先頭で再初期化CSを走らせる
  if (initCS_.IsReady()) {
    needsCSInit_ = true;
  }
  if (perFrameMapped_) {
    perFrameMapped_->maxParticles = maxParticles_;
  }
}
