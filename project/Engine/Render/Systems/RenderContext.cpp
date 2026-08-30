#include "RenderContext.h"

#include "Common/Log/Log.h"
#include "Dx12/Dx12Core.h"
#include "PipelineManager.h"
#include "Primitive/Primitive2D.h"
#include "Primitive/Primitive3D.h"
#include "Scene.h"
#include "Graphics/PostProcess/PostProcess.h"
#include "RenderCommon.h"
#include <algorithm>
#include <format>

namespace RC {

// ============================================================================
// シングルトン / 取得
// ============================================================================

RenderContext &RenderContext::GetInstance() {
  static RenderContext instance;
  return instance;
}

RenderContext &GetRenderContext() { return RenderContext::GetInstance(); }

// ============================================================================
// Init / Term
// ============================================================================

void RenderContext::Init(SceneContext &ctx) {
  if (initialized_) {
    return;
  }

  ctxRef_ = &ctx;
  postProcess_ = ctx.postProcess;
  device_ = ctx.core->GetDevice();
  srvHeap_ = &ctx.core->SRV();

  texMan_.Init(&ctx.core->SRVMan());
  spriteMan_.Init(device_.Get(), &texMan_);
  modelMan_.Init(device_.Get(), &texMan_);
  skydomeMan_.Init(device_.Get(), &texMan_);
  skyboxMan_.Init(device_.Get(), &texMan_);
  primitiveMeshMan_.Init(device_.Get(), &texMan_);

  dirLightMan_.Init(device_.Get());
  ptLightMan_.Init(device_.Get());
  spLightMan_.Init(device_.Get());
  arLightMan_.Init(device_.Get());

  // CameraCB
  cameraCB_ = CreateBufferResource(device_.Get(), sizeof(CameraCB),
                                   L"RenderContext::CameraCB");
  cameraCB_->Map(0, nullptr, reinterpret_cast<void **>(&cameraCBMapped_));

  // FogOverlayCB
  fogCB_ = CreateBufferResource(device_.Get(), sizeof(FogOverlayCB),
                                L"RenderContext::FogCB");
  fogCB_->Map(0, nullptr, reinterpret_cast<void **>(&fogCBMapped_));
  if (fogCBMapped_) {
    *fogCBMapped_ = FogOverlayCB{};
  }

  // ShadowCB
  shadowCB_ = CreateBufferResource(device_.Get(), sizeof(ShadowParams),
                                   L"RenderContext::ShadowCB");
  shadowCB_->Map(0, nullptr, reinterpret_cast<void **>(&shadowCBMapped_));
  if (shadowCBMapped_) {
    *shadowCBMapped_ = ShadowParams{};
  }

  // ShadowMap 初期化 (例: 2048x2048)
  shadowMap_.Create(ctx.core, 2048, 2048);

  view_ = MakeIdentity4x4();
  proj_ = MakeIdentity4x4();
  cl_ = nullptr;
  currentBlendMode_ = kBlendModeNone;

  // FrameResource 初期化（トリプルバッファ）
  for (uint32_t i = 0; i < FrameResource::kFrameCount; ++i) {
    frameResources_[i].Init(device_.Get(), i);
  }
  frameIndex_ = 0;

  initialized_ = true;

  // CS スキニング用パイプラインを ModelManager に注入
  if (ctxRef_ && ctxRef_->pipelineManager) {
    modelMan_.SetSkinningCS(ctxRef_->pipelineManager, &ctx.core->SRVMan());
  }

  // Dissolve ノイズテクスチャ初期化 (TextureManager が有効な状態で行う)
  if (postProcess_) {
    postProcess_->InitDissolveNoiseTextures();
  }
}

void RenderContext::Term() {
  if (!initialized_) {
    return;
  }

  // 残っている非同期タスクを全て待機
  WaitAllLoads();

  shadowMap_.Term();

  modelMan_.Term();
  skydomeMan_.Term();
  skyboxMan_.Term();
  primitiveMeshMan_.Term();
  spriteMan_.Term();

  dirLightMan_.Term();
  ptLightMan_.Term();
  spLightMan_.Term();
  arLightMan_.Term();

  prim2D_.reset();
  prim3D_.reset();

  for (uint32_t i = 0; i < FrameResource::kFrameCount; ++i) {
    frameResources_[i].Term();
  }

  if (cameraCB_) {
    if (cameraCBMapped_) {
      cameraCB_->Unmap(0, nullptr);
      cameraCBMapped_ = nullptr;
    }
    cameraCB_.Reset();
  }

  if (fogCB_) {
    if (fogCBMapped_) {
      fogCB_->Unmap(0, nullptr);
      fogCBMapped_ = nullptr;
    }
    fogCB_.Reset();
  }

  if (shadowCB_) {
    if (shadowCBMapped_) {
      shadowCB_->Unmap(0, nullptr);
      shadowCBMapped_ = nullptr;
    }
    shadowCB_.Reset();
  }

  texMan_.Term();
  TermWaterResources();

  ctxRef_ = nullptr;
  device_.Reset();
  srvHeap_ = nullptr;
  cl_ = nullptr;

  initialized_ = false;
}

// ============================================================================
// Camera
// ============================================================================

void RenderContext::SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
                              const Vector3 &camWorldPos) {
  view_ = view;
  proj_ = proj;

  if (cameraCBMapped_) {
    cameraCBMapped_->worldPos = camWorldPos;
    cameraCBMapped_->_pad = 0.0f;
  }

  if (postProcess_) {
    Matrix4x4 projInv = Inverse(proj);
    postProcess_->SetProjectionInverse(&projInv.m[0][0]);

    // Caustics がビュー空間 → ワールド空間の復元に使用する
    Matrix4x4 viewInv = Inverse(view);
    postProcess_->SetViewInverse(&viewInv.m[0][0]);
  }
}

// ============================================================================
// 描画パス実行
// ============================================================================

void RenderContext::PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // 前フレームの履歴を保存して今フレーム用をクリア
  lastCommandHistory_ = std::move(currentCommandHistory_);
  currentCommandHistory_.clear();

  cl_ = cl;
  ctxRef_ = &ctx;
  currentBlendMode_ = kBlendModeNone;

  auto *pso = GetPipeline("object3d", kBlendModeNone);
  if (!pso) {
    cl_ = nullptr;
    ctxRef_ = nullptr;
    return;
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  BindCameraCB();
  BindAllLightCBs();
  // BindShadow() は Execute3DCommands で通常パスの実行時のみバインドする

  modelMan_.ResetAllBatchCursors();

  // FrameResource: フレームインデックスを進めてリセット
  AdvanceFrame();
  CurrentFrame().Reset();

  // CS スキニングの一括Dispatch
  modelMan_.DispatchAllSkinning(cl, CurrentFrame());

  if (auto *prim = EnsurePrimitive3D()) {
    prim->BeginFrame(view_, proj_, kBlendModeNone);
  }
}

void RenderContext::PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  cl_ = cl;
  ctxRef_ = &ctx;
  currentBlendMode_ = kBlendModeNormal;

  // 3D コマンドキューを一括実行
  Execute3DCommands();

  // 2D Viewport / Scissor
  D3D12_VIEWPORT viewport{};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<float>(ctx.app->width);
  viewport.Height = static_cast<float>(ctx.app->height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  D3D12_RECT scissor{};
  scissor.left = 0;
  scissor.top = 0;
  scissor.right = static_cast<LONG>(ctx.app->width);
  scissor.bottom = static_cast<LONG>(ctx.app->height);

  cl->RSSetViewports(1, &viewport);
  cl->RSSetScissorRects(1, &scissor);

  auto *pso = GetPipeline("sprite", kBlendModeNormal);
  if (!pso) {
    cl_ = nullptr;
    ctxRef_ = nullptr;
    return;
  }

  // Primitive2D の BeginFrame
  if (auto *prim = EnsurePrimitive2D()) {
    prim->BeginFrame();
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ============================================================================
// テクスチャヘルパー
// ============================================================================

int RenderContext::LoadTex(const std::string &path, bool srgb) {
  if (!initialized_) {
    return -1;
  }
  return texMan_.LoadID(path, srgb);
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderContext::GetSrv(int texHandle) {
  if (!initialized_ || texHandle < 0) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }
  return texMan_.GetSrv(texHandle);
}

// ============================================================================
// PSO ヘルパー
// ============================================================================

GraphicsPipeline *RenderContext::GetPipeline(std::string_view prefix,
                                             BlendMode mode) {
  if (!ctxRef_ || !ctxRef_->pipelineManager) {
    return nullptr;
  }

  GraphicsPipeline *pso =
      ctxRef_->pipelineManager->Get(PipelineManager::MakeKey(prefix, mode));

  if (!pso && mode != kBlendModeNormal) {
    pso = ctxRef_->pipelineManager->Get(
        PipelineManager::MakeKey(prefix, kBlendModeNormal));
  }

  return pso;
}

GraphicsPipeline *RenderContext::BindPipeline(std::string_view prefix) {
  if (!cl_ || !ctxRef_) {
    return nullptr;
  }

  std::string_view actualPrefix = prefix;
  if (isShadowPass_) {
    // スキニング用・インスタンシング用などの対応
    if (prefix.find("skin") != std::string_view::npos) {
      actualPrefix = "shadow_skin";
    } else if (prefix.find("inst") != std::string_view::npos) {
      actualPrefix = "shadow_inst";
    } else {
      actualPrefix = "shadow";
    }
  }

  auto *pso = GetPipeline(actualPrefix, currentBlendMode_);
  if (!pso) {
    return nullptr;
  }

  cl_->SetGraphicsRootSignature(pso->Root());
  cl_->SetPipelineState(pso->PSO());

  // シャドウマップ（またはHazard回避用ダミー）のバインド
  if (actualPrefix.find("object3d") != std::string_view::npos ||
      actualPrefix.find("shadow") != std::string_view::npos) {
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv = {};
    if (isShadowPass_) {
      int dummyTex = texMan_.LoadID("Resources/white1x1.png", false);
      if (dummyTex >= 0) {
        shadowSrv = texMan_.GetSrv(dummyTex);
      }
    } else if (shadowMap_.GetResource() != nullptr && ctxRef_ && ctxRef_->core) {
      shadowSrv = ctxRef_->core->SRV().GPUAt(shadowMap_.GetSrvIndex());
    }

    if (shadowSrv.ptr != 0) {
      // GraphicsPipeline のルートシグネチャ定義を統一したため、Skinning でも 11 番スロットに ShadowMap が来る
      UINT srvSlot = 11;
      cl_->SetGraphicsRootDescriptorTable(srvSlot, shadowSrv);
    }

    if (shadowCB_) {
      UINT cbvSlot = (actualPrefix.find("skin") != std::string_view::npos) ? 13 : 12;
      cl_->SetGraphicsRootConstantBufferView(cbvSlot, shadowCB_->GetGPUVirtualAddress());
    }
  }

  return pso;
}

void RenderContext::BindCameraCB() {
  if (!cl_ || !cameraCB_) {
    return;
  }
  cl_->SetGraphicsRootConstantBufferView(4,
                                         cameraCB_->GetGPUVirtualAddress());
}

void RenderContext::BindAllLightCBs() {
  if (!cl_) {
    return;
  }

  // PointLightCB → RootParam[5]
  if (const D3D12_GPU_VIRTUAL_ADDRESS addr = ptLightMan_.GetCBAddress()) {
    cl_->SetGraphicsRootConstantBufferView(5, addr);
  }
  // SpotLightCB → RootParam[6]
  if (const D3D12_GPU_VIRTUAL_ADDRESS addr = spLightMan_.GetCBAddress()) {
    cl_->SetGraphicsRootConstantBufferView(6, addr);
  }
  // AreaLightCB → RootParam[7]
  if (const D3D12_GPU_VIRTUAL_ADDRESS addr = arLightMan_.GetCBAddress()) {
    cl_->SetGraphicsRootConstantBufferView(7, addr);
  }

  // EnvironmentMap → RootParam[8]
  BindEnvironmentMap();
}

void RenderContext::BindEnvironmentMap() {
  if (!cl_ || environmentMapSrv_.ptr == 0) {
    return;
  }
  cl_->SetGraphicsRootDescriptorTable(8, environmentMapSrv_);
}

void RenderContext::UpdateShadowParams(const ShadowParams& params) {
  if (shadowCBMapped_) {
    *shadowCBMapped_ = params;

    // PCF のタップ間隔はシャドウマップの実解像度から求めるため、
    // 呼び出し側の値に関わらずここで上書きする（シーン側が解像度を知る必要をなくす）
    const uint32_t w = shadowMap_.GetWidth();
    const uint32_t h = shadowMap_.GetHeight();
    shadowCBMapped_->shadowMapTexelSize = {
        (w > 0) ? 1.0f / static_cast<float>(w) : 0.0f,
        (h > 0) ? 1.0f / static_cast<float>(h) : 0.0f};

    // 確認用の上書き（C-04）。
    // シーン側は毎フレーム pcfRadius / bias をハードコード値で作り直すので、
    // 外から効かせるにはテクセルサイズと同じくここで塗り替えるしかない。
    // enabled が false のあいだは一切触らないため、通常動作は変わらない。
    if (shadowDebug_.enabled) {
      shadowCBMapped_->pcfRadius = shadowDebug_.pcfRadius;
      shadowCBMapped_->bias = shadowDebug_.bias;
      shadowCBMapped_->color.w = shadowDebug_.darkness;
      if (shadowDebug_.forceDisable) {
        shadowCBMapped_->shadowMapEnabled = 0u;
      }
    }
  }
}

void RenderContext::BindShadow() {
  // BindPipeline 側でバインドするようになったため、ここでは何もしません。
}

void RenderContext::BeginShadowPass() {
  if (!cl_) return;

  // Viewport / Scissor をシャドウマップに合わせる
  D3D12_VIEWPORT viewport{};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<float>(shadowMap_.GetWidth());
  viewport.Height = static_cast<float>(shadowMap_.GetHeight());
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  D3D12_RECT scissor{};
  scissor.left = 0;
  scissor.top = 0;
  scissor.right = shadowMap_.GetWidth();
  scissor.bottom = shadowMap_.GetHeight();

  cl_->RSSetViewports(1, &viewport);
  cl_->RSSetScissorRects(1, &scissor);

  shadowMap_.BindAndClear(cl_);
  isShadowPass_ = true;

  // シャドウ用パイプラインをバインド
  auto *pso = GetPipeline("shadow", kBlendModeNone);
  if (pso) {
    cl_->SetGraphicsRootSignature(pso->Root());
    cl_->SetPipelineState(pso->PSO());
    cl_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Dummy texture binding is now handled inside BindPipeline
  }
}

void RenderContext::EndShadowPass() {
  if (!cl_) return;
  isShadowPass_ = false;
  shadowMap_.TransitionToSRV(cl_);

  if (ctxRef_ && ctxRef_->app) {
    auto rtv = ctxRef_->currentRTV;
    auto dsv = ctxRef_->currentDSV;
    if (rtv.ptr != 0) {
      cl_->OMSetRenderTargets(1, &rtv, FALSE, (dsv.ptr != 0) ? &dsv : nullptr);
    }

    auto w = static_cast<float>(ctxRef_->app->width);
    auto h = static_cast<float>(ctxRef_->app->height);
    D3D12_VIEWPORT vp = {0.0f, 0.0f, w, h, 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};
    cl_->RSSetViewports(1, &vp);
    cl_->RSSetScissorRects(1, &scissor);
  }
}

// ============================================================================
// Primitive 遅延生成
// ============================================================================

Primitive2D *RenderContext::EnsurePrimitive2D() {
  if (!initialized_ || !device_ || !ctxRef_ || !ctxRef_->app) {
    return nullptr;
  }

  const float w = static_cast<float>(ctxRef_->app->width);
  const float h = static_cast<float>(ctxRef_->app->height);

  if (!prim2D_) {
    prim2D_ = std::make_unique<Primitive2D>();
    prim2D_->Initialize(device_.Get(), w, h);
  } else {
    prim2D_->SetScreenSize(w, h);
  }

  return prim2D_.get();
}

Primitive3D *RenderContext::EnsurePrimitive3D() {
  if (!initialized_ || !device_ || !ctxRef_) {
    return nullptr;
  }

  if (!prim3D_) {
    prim3D_ = std::make_unique<Primitive3D>();
    prim3D_->Initialize(device_.Get());
  }
  return prim3D_.get();
}

// ============================================================================
// Fog CB
// ============================================================================

void RenderContext::UpdateFogCB(float timeSec, float intensity, float scale,
                                float speed, const Vector2 &wind,
                                float feather, float bottomBias) {
  if (fogCBMapped_) {
    fogCBMapped_->timeSec = timeSec;
    fogCBMapped_->intensity = intensity;
    fogCBMapped_->scale = scale;
    fogCBMapped_->speed = speed;
    fogCBMapped_->wind = wind;
    fogCBMapped_->feather = feather;
    fogCBMapped_->bottomBias = bottomBias;
  }
}

void RenderContext::SetFogColor(const Vector4 &color) {
  if (fogCBMapped_) {
    fogCBMapped_->color = color;
  }
}

void RenderContext::PushPrimitive3DCommand(bool depth, uint32_t start,
                                           uint32_t count, uint64_t sortKey) {
  if (count == 0)
    return;

  if (!commandQueue3D_.empty()) {
    auto &last = commandQueue3D_.back();
    if (last.type == RenderCommand3D::Primitive && last.primDepth == depth &&
        last.sortKey == sortKey) {
      last.primCount += count;
      return;
    }
  }

  RenderCommand3D cmd;
  cmd.type = RenderCommand3D::Primitive;
  cmd.sortKey = sortKey;
  cmd.primDepth = depth;
  cmd.primStart = start;
  cmd.primCount = count;
  commandQueue3D_.push_back(std::move(cmd));
}

void RenderContext::Execute3DCommands() {
  if (!cl_) {
    return;
  }

  // BindShadow(); // BindPipeline に移行したため不要


  // 最初の1フレームだけ自動でダンプを要求する
  static bool s_firstDump = false;
  if (!s_firstDump) {
    dumpCommandOrder_ = true;
    s_firstDump = true;
  }

  // 0) ソートキーで安定ソート（sortKey==0 は push 順を維持）
  std::stable_sort(commandQueue3D_.begin(), commandQueue3D_.end(),
                   [](const RenderCommand3D &a, const RenderCommand3D &b) {
                     return a.sortKey < b.sortKey;
                   });

  if (dumpCommandOrder_) {
    Log::Print("[RenderContext] --- Execute3DCommands Order Dump ---");
    for (size_t i = 0; i < commandQueue3D_.size(); ++i) {
      const auto& cmd = commandQueue3D_[i];
      uint8_t layer = static_cast<uint8_t>(cmd.sortKey >> 56);
      uint16_t psoHash = static_cast<uint16_t>((cmd.sortKey >> 40) & 0xFFFF);
      uint16_t texHash = static_cast<uint16_t>((cmd.sortKey >> 24) & 0xFFFF);
      uint32_t depth24 = static_cast<uint32_t>(cmd.sortKey & 0x00FFFFFF);
      std::string layerStr = (layer == 0) ? "Opaque" : (layer == 1) ? "Translucent" : (layer == 2) ? "Glass" : (layer == 3) ? "Overlay" : "Unknown";

      if (cmd.type == RenderCommand3D::Primitive) {
        Log::Print(std::format("  [{}] Primitive (Depth:{}) - Layer: {}({}), Depth24: {}, PSO: {:04X}, Tex: {:04X}", 
            i, cmd.primDepth, layer, layerStr, depth24, psoHash, texHash));
      } else {
        std::string displayName = cmd.debugName ? cmd.debugName : "Unknown";
        if (cmd.debugIndex >= 0) {
          std::string resourceName = "";
          if (displayName.find("Model") != std::string::npos) {
            if (auto* m = modelMan_.Get(cmd.debugIndex)) {
              resourceName = std::filesystem::path(m->GetFilePath()).filename().string();
            }
          } else if (displayName.find("Sprite") != std::string::npos) {
            if (auto* s = spriteMan_.Get(cmd.debugIndex)) {
              resourceName = std::filesystem::path(s->GetFilePath()).filename().string();
            }
          }
          if (!resourceName.empty()) {
            displayName += " [" + resourceName + "]";
          }
          
          if (cmd.sortKey == 0) {
            Log::Print(std::format("  [{}] {} [{}]", i, displayName, cmd.debugIndex));
          } else {
            Log::Print(std::format("  [{}] {} [{}] - Layer: {}({}), Depth24: {}, PSO: {:04X}, Tex: {:04X}", 
              i, displayName, cmd.debugIndex, layer, layerStr, depth24, psoHash, texHash));
          }
        } else {
          if (cmd.sortKey == 0) {
            Log::Print(std::format("  [{}] {}", i, displayName));
          } else {
            Log::Print(std::format("  [{}] {} - Layer: {}({}), Depth24: {}, PSO: {:04X}, Tex: {:04X}", 
              i, displayName, layer, layerStr, depth24, psoHash, texHash));
          }
        }
      }
    }
    Log::Print("[RenderContext] ------------------------------------");
    dumpCommandOrder_ = false;
  }

  // 1) プリミティブの頂点転送 (メインパスのみ)
  if (prim3D_ && prim3D_->HasAny() && !isShadowPass_) {
    prim3D_->TransferVertices();
  }

  std::vector<RenderCommand3D> deferredCommands;

  // 2) キューに積まれたコマンドを順に実行
  for (auto &cmd : commandQueue3D_) {
    // シャドウパス中はプリミティブ描画（デバッグ線など）をスキップし、メインパスへ持ち越す
    if (cmd.type == RenderCommand3D::Primitive && isShadowPass_) {
      deferredCommands.push_back(std::move(cmd));
      continue;
    }

    // 履歴に追加
    if (cmd.type == RenderCommand3D::Primitive) {
      AddCommandHistory("Primitive3D", -1, cmd.sortKey);
    } else {
      AddCommandHistory(cmd.debugName ? cmd.debugName : "Unknown", cmd.debugIndex, cmd.sortKey);
    }

    if (cmd.type == RenderCommand3D::Primitive) {
      if (prim3D_) {
        std::string psoName = cmd.primDepth ? "primitive3d" : "primitive3d_nodepth";
        if (BindPipeline(psoName)) {
          prim3D_->DrawRange(cl_, cmd.primDepth, cmd.primStart, cmd.primCount);
        }
      }
    } else if (cmd.func) {
      cmd.func(cl_);
    }
  }

  // 3) 実行後の後始末
  commandQueue3D_ = std::move(deferredCommands);
  if (prim3D_ && !isShadowPass_) {
    prim3D_->Clear();
  }
}

void RenderContext::AddLoadingTask(std::future<void> &&task) {
  std::lock_guard<std::mutex> lock(mtxTasks_);
  ongoingTasks_.push_back(std::move(task));
}

void RenderContext::WaitAllLoads() {
  std::vector<std::future<void>> tasks;
  {
    std::lock_guard<std::mutex> lock(mtxTasks_);
    tasks = std::move(ongoingTasks_);
  }

  for (auto &f : tasks) {
    if (f.valid()) {
      f.get();
    }
  }
}

} // namespace RC
