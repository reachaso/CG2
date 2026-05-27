#include "PostProcess.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/GraphicsPipeline/GraphicsPipeline.h"
#include "Dx12/PipelineManager.h"
#include "Graphics/Texture/RenderTexture/RenderTexture.h"
#include "Render/RenderContext.h"
#include "Common/Log/Log.h"

#include <filesystem>
#include <algorithm>
#include <format>

namespace {
const char* ToString(PostEffectType type) {
  switch (type) {
  case PostEffectType::Grayscale: return "Grayscale";
  case PostEffectType::Sepia:     return "Sepia";
  case PostEffectType::Vignette:  return "Vignette";
  case PostEffectType::BoxFilter: return "BoxFilter";
  case PostEffectType::GaussianFilter: return "GaussianFilter";
  case PostEffectType::DepthBasedOutline: return "DepthBasedOutline";
  case PostEffectType::RadialBlur: return "RadialBlur";
  case PostEffectType::Dissolve:   return "Dissolve";
  case PostEffectType::RandomNoise:return "RandomNoise";
  case PostEffectType::None:      return "None";
  default:                        return "Unknown";
  }
}

std::string ActiveEffectsToString(const std::vector<PostEffectType>& effects) {
  if (effects.empty()) return "None";
  std::string result;
  for (size_t i = 0; i < effects.size(); ++i) {
    if (i > 0) result += " -> ";
    result += ToString(effects[i]);
  }
  return result;
}
} // namespace

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

void PostProcess::Initialize(Dx12Core *dxCore,
                             PipelineManager *pipelineManager,
                             uint32_t width, uint32_t height) {
  dxCore_ = dxCore;
  pipelineManager_ = pipelineManager;
  width_ = width;
  height_ = height;

  // 各パイプラインを取得
  pipelineCopy_ = pipelineManager_->Get("copyimage.none");
  assert(pipelineCopy_ && "Failed to get copyimage pipeline");

  pipelineGrayscale_ = pipelineManager_->Get("grayscale.none");
  assert(pipelineGrayscale_ && "Failed to get grayscale pipeline");

  pipelineSepia_ = pipelineManager_->Get("sepia.none");
  assert(pipelineSepia_ && "Failed to get sepia pipeline");

  pipelineVignette_ = pipelineManager_->Get("vignette.none");
  assert(pipelineVignette_ && "Failed to get vignette pipeline");

  pipelineBoxFilter_ = pipelineManager_->Get("boxfilter.none");
  assert(pipelineBoxFilter_ && "Failed to get boxfilter pipeline");

  pipelineGaussianFilter_ = pipelineManager_->Get("gaussianfilter.none");
  assert(pipelineGaussianFilter_ && "Failed to get gaussianfilter pipeline");

  pipelineDepthBasedOutline_ = pipelineManager_->Get("depthbasedoutline.none");
  assert(pipelineDepthBasedOutline_ && "Failed to get depthbasedoutline pipeline");

  pipelineRadialBlur_ = pipelineManager_->Get("radialblur.none");
  assert(pipelineRadialBlur_ && "Failed to get radialblur pipeline");

  pipelineDissolve_ = pipelineManager_->Get("dissolve.none");
  assert(pipelineDissolve_ && "Failed to get dissolve pipeline");

  pipelineRandom_ = pipelineManager_->Get("random.none");
  assert(pipelineRandom_ && "Failed to get random pipeline");

  // CBuffer 初期化
  D3D12_HEAP_PROPERTIES uploadHeap{D3D12_HEAP_TYPE_UPLOAD};
  D3D12_RESOURCE_DESC cbDesc{};
  cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  cbDesc.Width = (sizeof(MaterialData) + 255) & ~255;
  cbDesc.Height = 1;
  cbDesc.DepthOrArraySize = 1;
  cbDesc.MipLevels = 1;
  cbDesc.Format = DXGI_FORMAT_UNKNOWN;
  cbDesc.SampleDesc.Count = 1;
  cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  HRESULT hr = dxCore_->GetDevice()->CreateCommittedResource(
      &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&cbufferMaterial_));
  assert(SUCCEEDED(hr));
  cbufferMaterial_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
  
  if (mappedMaterial_) {
    memcpy(mappedMaterial_->outlineColor, outlineColor_, sizeof(float) * 4);
    mappedMaterial_->outlineWeight = outlineWeight_;
    mappedMaterial_->outlineThickness = outlineThickness_;
    mappedMaterial_->outlineMode = outlineMode_;
  }

  // Dissolve CBuffer 初期化
  {
    D3D12_HEAP_PROPERTIES uploadHeap{D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC cbDescDissolve{};
    cbDescDissolve.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDescDissolve.Width = (sizeof(DissolveData) + 255) & ~255;
    cbDescDissolve.Height = 1;
    cbDescDissolve.DepthOrArraySize = 1;
    cbDescDissolve.MipLevels = 1;
    cbDescDissolve.Format = DXGI_FORMAT_UNKNOWN;
    cbDescDissolve.SampleDesc.Count = 1;
    cbDescDissolve.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDescDissolve.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = dxCore_->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDescDissolve,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&cbufferDissolve_));
    assert(SUCCEEDED(hr));
    cbufferDissolve_->Map(0, nullptr, reinterpret_cast<void **>(&mappedDissolve_));

    if (mappedDissolve_) {
      mappedDissolve_->edgeColor[0] = dissolveEdgeColor_[0];
      mappedDissolve_->edgeColor[1] = dissolveEdgeColor_[1];
      mappedDissolve_->edgeColor[2] = dissolveEdgeColor_[2];
      mappedDissolve_->edgeColor[3] = 1.0f;
      mappedDissolve_->baseColor[0] = dissolveBaseColor_[0];
      mappedDissolve_->baseColor[1] = dissolveBaseColor_[1];
      mappedDissolve_->baseColor[2] = dissolveBaseColor_[2];
      mappedDissolve_->baseColor[3] = dissolveBaseColor_[3];
      mappedDissolve_->threshold = dissolveThreshold_;
      mappedDissolve_->edgeRange = dissolveEdgeRange_;
    }
  }

  // ノイズテクスチャはRC::Init後に遅延初期化される (InitDissolveNoiseTextures)

  // RandomNoise CBuffer 初期化
  {
    D3D12_HEAP_PROPERTIES uploadHeap{D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC cbDescRandom{};
    cbDescRandom.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDescRandom.Width = (sizeof(RandomNoiseData) + 255) & ~255;
    cbDescRandom.Height = 1;
    cbDescRandom.DepthOrArraySize = 1;
    cbDescRandom.MipLevels = 1;
    cbDescRandom.Format = DXGI_FORMAT_UNKNOWN;
    cbDescRandom.SampleDesc.Count = 1;
    cbDescRandom.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDescRandom.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = dxCore_->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDescRandom,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&cbufferRandom_));
    assert(SUCCEEDED(hr));
    cbufferRandom_->Map(0, nullptr, reinterpret_cast<void **>(&mappedRandom_));

    if (mappedRandom_) {
      mappedRandom_->color[0] = randomColor_[0];
      mappedRandom_->color[1] = randomColor_[1];
      mappedRandom_->color[2] = randomColor_[2];
      mappedRandom_->color[3] = 1.0f;
      mappedRandom_->time = randomTime_;
      mappedRandom_->intensity = randomIntensity_;
    }
  }
}

void PostProcess::UpdateTime(float deltaTime) {
  randomTime_ += deltaTime;
  if (mappedRandom_) {
    mappedRandom_->time = randomTime_;
  }
}

void PostProcess::SetProjectionInverse(const float* projInv16) {
  if (mappedMaterial_) {
    memcpy(mappedMaterial_->projectionInverse, projInv16, sizeof(float) * 16);
  }
}

void PostProcess::SetOutlineColor(const float color[4]) {
  memcpy(outlineColor_, color, sizeof(float) * 4);
  if (mappedMaterial_) {
    memcpy(mappedMaterial_->outlineColor, color, sizeof(float) * 4);
  }
}

void PostProcess::SetOutlineWeight(float weight) {
  outlineWeight_ = weight;
  if (mappedMaterial_) {
    mappedMaterial_->outlineWeight = weight;
  }
}

void PostProcess::SetOutlineThickness(float thickness) {
  outlineThickness_ = thickness;
  if (mappedMaterial_) {
    mappedMaterial_->outlineThickness = thickness;
  }
}

void PostProcess::SetOutlineMode(int mode) {
  outlineMode_ = mode;
  if (mappedMaterial_) {
    mappedMaterial_->outlineMode = mode;
  }
}

// ============================================================================
// Dissolve パラメータ
// ============================================================================

void PostProcess::SetDissolveThreshold(float threshold) {
  dissolveThreshold_ = threshold;
  if (mappedDissolve_) {
    mappedDissolve_->threshold = threshold;
  }
}

void PostProcess::SetDissolveEdgeColor(float r, float g, float b) {
  dissolveEdgeColor_[0] = r;
  dissolveEdgeColor_[1] = g;
  dissolveEdgeColor_[2] = b;
  if (mappedDissolve_) {
    mappedDissolve_->edgeColor[0] = r;
    mappedDissolve_->edgeColor[1] = g;
    mappedDissolve_->edgeColor[2] = b;
  }
}

void PostProcess::SetDissolveBaseColor(float r, float g, float b, float a) {
  dissolveBaseColor_[0] = r;
  dissolveBaseColor_[1] = g;
  dissolveBaseColor_[2] = b;
  dissolveBaseColor_[3] = a;
  if (mappedDissolve_) {
    mappedDissolve_->baseColor[0] = r;
    mappedDissolve_->baseColor[1] = g;
    mappedDissolve_->baseColor[2] = b;
    mappedDissolve_->baseColor[3] = a;
  }
}

void PostProcess::SetDissolveEdgeRange(float range) {
  dissolveEdgeRange_ = range;
  if (mappedDissolve_) {
    mappedDissolve_->edgeRange = range;
  }
}

void PostProcess::SetDissolveNoiseIndex(int index) {
  if (!dissolveNoiseTextures_.empty()) {
    dissolveNoiseIndex_ = index % static_cast<int>(dissolveNoiseTextures_.size());
  }
}

void PostProcess::SetRandomNoiseIntensity(float intensity) {
  randomIntensity_ = intensity;
  if (mappedRandom_) mappedRandom_->intensity = intensity;
}

void PostProcess::SetRandomNoiseColor(float r, float g, float b) {
  randomColor_[0] = r;
  randomColor_[1] = g;
  randomColor_[2] = b;
  if (mappedRandom_) {
    mappedRandom_->color[0] = r;
    mappedRandom_->color[1] = g;
    mappedRandom_->color[2] = b;
  }
}

// ============================================================================
void PostProcess::InitDissolveNoiseTextures() {
  if (dissolveNoiseInitialized_) return;
  dissolveNoiseInitialized_ = true;

  // Resources/noise/ フォルダからPNGをリストアップ
  const std::string noiseDir = "Resources/noise";
  if (!std::filesystem::exists(noiseDir)) return;

  auto& rc = RC::RenderContext::GetInstance();
  auto& texMan = rc.Textures();

  for (const auto& entry : std::filesystem::directory_iterator(noiseDir)) {
    if (!entry.is_regular_file()) continue;
    const auto ext = entry.path().extension().string();
    if (ext != ".png" && ext != ".PNG") continue;

    NoiseEntry ne;
    ne.path = entry.path().string();
    ne.name = entry.path().stem().string();
    // srgb=false: マスクはリニア値として扱う
    ne.srv = texMan.Load(ne.path, false);
    dissolveNoiseTextures_.push_back(ne);
    Log::Print(std::format("[PostProcess] Dissolve noise loaded: {}", ne.name));
  }

  if (dissolveNoiseTextures_.empty()) {
    Log::Print("[PostProcess] Warning: No noise textures found in Resources/noise/");
  }
}

// ============================================================================
// エフェクト操作
// ============================================================================

void PostProcess::SetEffect(PostEffectType type) {
  activeEffects_.clear();
  if (type != PostEffectType::None) {
    activeEffects_.push_back(type);
    Log::Print(std::format("[PostProcess] SetEffect: {} (Active: {})", ToString(type), ActiveEffectsToString(activeEffects_)));
  } else {
    Log::Print(std::format("[PostProcess] SetEffect: None (Active: {})", ActiveEffectsToString(activeEffects_)));
  }
}

PostEffectType PostProcess::GetEffect() const {
  return activeEffects_.empty() ? PostEffectType::None : activeEffects_.front();
}

void PostProcess::AddEffect(PostEffectType type) {
  if (type == PostEffectType::None) {
    return;
  }
  if (!HasEffect(type)) {
    activeEffects_.push_back(type);
    Log::Print(std::format("[PostProcess] AddEffect: {} (Active: {})", ToString(type), ActiveEffectsToString(activeEffects_)));
  }
}

void PostProcess::RemoveEffect(PostEffectType type) {
  auto it = std::find(activeEffects_.begin(), activeEffects_.end(), type);
  if (it != activeEffects_.end()) {
    activeEffects_.erase(it);
    Log::Print(std::format("[PostProcess] RemoveEffect: {} (Active: {})", ToString(type), ActiveEffectsToString(activeEffects_)));
  }
}

void PostProcess::ClearEffects() { 
  if (!activeEffects_.empty()) {
    activeEffects_.clear();
    Log::Print(std::format("[PostProcess] ClearEffects (Active: {})", ActiveEffectsToString(activeEffects_)));
  }
}

bool PostProcess::HasEffect(PostEffectType type) const {
  return std::find(activeEffects_.begin(), activeEffects_.end(), type) !=
         activeEffects_.end();
}

// ============================================================================
// パイプライン選択
// ============================================================================

GraphicsPipeline *PostProcess::GetPipelineForEffect(PostEffectType type) {
  switch (type) {
  case PostEffectType::Grayscale:
    return pipelineGrayscale_;
  case PostEffectType::Sepia:
    return pipelineSepia_;
  case PostEffectType::Vignette:
    return pipelineVignette_;
  case PostEffectType::BoxFilter:
    return pipelineBoxFilter_;
  case PostEffectType::GaussianFilter:
    return pipelineGaussianFilter_;
  case PostEffectType::DepthBasedOutline:
    return pipelineDepthBasedOutline_;
  case PostEffectType::RadialBlur:
    return pipelineRadialBlur_;
  case PostEffectType::Dissolve:
    return pipelineDissolve_;
  case PostEffectType::RandomNoise:
    return pipelineRandom_;
  case PostEffectType::None:
  default:
    return pipelineCopy_;
  }
}

// ============================================================================
// ピンポンテクスチャ（マルチパス用・遅延初期化）
// ============================================================================

void PostProcess::EnsurePingPongTextures() {
  if (pingPongInitialized_) {
    return;
  }
  pingPongA_ = std::make_unique<RenderTexture>();
  pingPongA_->Initialize(dxCore_, width_, height_);

  pingPongB_ = std::make_unique<RenderTexture>();
  pingPongB_->Initialize(dxCore_, width_, height_);

  pingPongInitialized_ = true;
}

// ============================================================================
// 1パス描画
// ============================================================================

void PostProcess::DrawSinglePass(ID3D12GraphicsCommandList *cmdList,
                                 D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                                 GraphicsPipeline *pipeline,
                                 PostEffectType effectType) {
  assert(pipeline);
  cmdList->SetGraphicsRootSignature(pipeline->Root());
  cmdList->SetPipelineState(pipeline->PSO());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootSignatureType::PostProcess → params[0] が SRV table (t0)
  cmdList->SetGraphicsRootDescriptorTable(0, srcSRV);

  // params[1] が RootConstants (b0) — エフェクト固有パラメータ
  struct PostEffectConstants {
    uint32_t param0;
    uint32_t param1;
    uint32_t param2;
    uint32_t param3;
  } constants = { 0, 0, 0, 0 };

  if (effectType == PostEffectType::BoxFilter) {
    constants.param0 = static_cast<uint32_t>(boxFilterK_);
  } else if (effectType == PostEffectType::GaussianFilter) {
    constants.param0 = static_cast<uint32_t>(gaussianFilterK_);
    constants.param1 = *(uint32_t *)&gaussianSigma_;
  } else if (effectType == PostEffectType::RadialBlur) {
    constants.param0 = *(uint32_t *)&radialBlurCenter_.x;
    constants.param1 = *(uint32_t *)&radialBlurCenter_.y;
    constants.param2 = *(uint32_t *)&radialBlurWidth_;
    constants.param3 = static_cast<uint32_t>(radialBlurSamples_);
  }

  cmdList->SetGraphicsRoot32BitConstants(1, 4, &constants, 0);

  if (effectType == PostEffectType::DepthBasedOutline) {
    if (!depthSrv_.IsValid()) {
      depthSrv_ = dxCore_->SRVMan().CreateTexture2D(
          dxCore_->GetDepthResource(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 1);
    }
    // params[2]: t1
    cmdList->SetGraphicsRootDescriptorTable(2, depthSrv_.gpu);
    // params[3]: b1
    cmdList->SetGraphicsRootConstantBufferView(3, cbufferMaterial_->GetGPUVirtualAddress());
  }

  if (effectType == PostEffectType::Dissolve) {
    // params[2]: t1 (ノイズマスクテクスチャ)
    if (!dissolveNoiseTextures_.empty()) {
      int idx = dissolveNoiseIndex_ % static_cast<int>(dissolveNoiseTextures_.size());
      cmdList->SetGraphicsRootDescriptorTable(2, dissolveNoiseTextures_[idx].srv);
    }
    // params[3]: b1 (DissolveParams CBuffer)
    cmdList->SetGraphicsRootConstantBufferView(3, cbufferDissolve_->GetGPUVirtualAddress());
  }

  if (effectType == PostEffectType::RandomNoise) {
    // params[3]: b1 (RandomNoise CBuffer)
    cmdList->SetGraphicsRootConstantBufferView(3, cbufferRandom_->GetGPUVirtualAddress());
  }

  // 全画面三角形（頂点バッファなし、SV_VertexID 使用）
  cmdList->DrawInstanced(3, 1, 0, 0);
}

// ============================================================================
// メイン描画（マルチパス対応）
// ============================================================================

void PostProcess::Draw(ID3D12GraphicsCommandList *cmdList,
                       const RenderTexture &renderTexture,
                       RenderTexture *dstTexture) {
  // ビューポート・シザー（全パス共通サイズ）
  D3D12_VIEWPORT vp{};
  vp.Width = static_cast<float>(width_);
  vp.Height = static_cast<float>(height_);
  vp.MaxDepth = 1.0f;
  D3D12_RECT sr{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};

  // 最終描画先を設定するラムダ
  auto SetFinalRenderTarget = [&]() {
    if (dstTexture) {
      dstTexture->TransitionToRenderTarget(cmdList);
      D3D12_CPU_DESCRIPTOR_HANDLE rtv = dstTexture->GetRTV();
      cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      cmdList->RSSetViewports(1, &vp);
      cmdList->RSSetScissorRects(1, &sr);
    } else {
      D3D12_CPU_DESCRIPTOR_HANDLE backRtv = dxCore_->CurrentRTV();
      cmdList->OMSetRenderTargets(1, &backRtv, FALSE, nullptr);
      dxCore_->ResetViewportScissorToBackbuffer(width_, height_);
    }
  };

  // --- エフェクトなし：そのままコピー ---
  if (activeEffects_.empty()) {
    SetFinalRenderTarget();
    DrawSinglePass(cmdList, renderTexture.GetSRVGPU(), pipelineCopy_, PostEffectType::None);
    return;
  }

  // --- 1エフェクト：シングルパス ---
  if (activeEffects_.size() == 1) {
    SetFinalRenderTarget();
    DrawSinglePass(cmdList, renderTexture.GetSRVGPU(),
                   GetPipelineForEffect(activeEffects_[0]),
                   activeEffects_[0]);
    return;
  }

  // --- 2エフェクト以上：マルチパス（ピンポン） ---
  EnsurePingPongTextures();

  RenderTexture *textures[2] = {pingPongA_.get(), pingPongB_.get()};
  const size_t count = activeEffects_.size();

  for (size_t i = 0; i < count; ++i) {
    const bool isFirst = (i == 0);
    const bool isLast = (i == count - 1);

    // ── ソースSRV ──
    D3D12_GPU_DESCRIPTOR_HANDLE srcSRV;
    if (isFirst) {
      // 最初のパス：入力テクスチャ（App の RenderTexture、すでに SRV 状態）
      srcSRV = renderTexture.GetSRVGPU();
    } else {
      // 前パスの出力をソースにする
      RenderTexture *prevDst = textures[(i - 1) % 2];
      prevDst->TransitionToShaderResource(cmdList);
      srcSRV = prevDst->GetSRVGPU();
    }

    // ── デスティネーション ──
    if (!isLast) {
      // 中間パス：ピンポンテクスチャに書き込む
      RenderTexture *dst = textures[i % 2];
      dst->TransitionToRenderTarget(cmdList);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv = dst->GetRTV();
      cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      cmdList->RSSetViewports(1, &vp);
      cmdList->RSSetScissorRects(1, &sr);
    } else {
      // 最終パス：バックバッファまたは指定されたテクスチャに書き込む
      SetFinalRenderTarget();
    }

    // ── 描画 ──
    DrawSinglePass(cmdList, srcSRV,
                   GetPipelineForEffect(activeEffects_[i]),
                   activeEffects_[i]);
  }
}

// ============================================================================
// ImGui
// ============================================================================

void PostProcess::DrawImGui([[maybe_unused]] const char *label) {
#if RC_ENABLE_IMGUI
  if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
    // 各エフェクトのチェックボックス
    bool grayscale = HasEffect(PostEffectType::Grayscale);
    bool sepia = HasEffect(PostEffectType::Sepia);
    bool vignette = HasEffect(PostEffectType::Vignette);

    if (ImGui::Checkbox("Grayscale", &grayscale)) {
      if (grayscale) {
        AddEffect(PostEffectType::Grayscale);
      } else {
        RemoveEffect(PostEffectType::Grayscale);
      }
    }

    if (ImGui::Checkbox("Sepia", &sepia)) {
      if (sepia) {
        AddEffect(PostEffectType::Sepia);
      } else {
        RemoveEffect(PostEffectType::Sepia);
      }
    }

    if (ImGui::Checkbox("Vignette", &vignette)) {
      if (vignette) {
        AddEffect(PostEffectType::Vignette);
      } else {
        RemoveEffect(PostEffectType::Vignette);
      }
    }

    bool boxFilter = HasEffect(PostEffectType::BoxFilter);
    if (ImGui::Checkbox("BoxFilter", &boxFilter)) {
      if (boxFilter) {
        AddEffect(PostEffectType::BoxFilter);
      } else {
        RemoveEffect(PostEffectType::BoxFilter);
      }
    }

    // BoxFilter が有効なとき K スライダーを表示
    if (boxFilter) {
      ImGui::Indent();
      ImGui::SliderInt("K", &boxFilterK_, 1, 10);
      ImGui::Unindent();
    }

    bool gaussianFilter = HasEffect(PostEffectType::GaussianFilter);
    if (ImGui::Checkbox("GaussianFilter", &gaussianFilter)) {
      if (gaussianFilter) {
        AddEffect(PostEffectType::GaussianFilter);
      } else {
        RemoveEffect(PostEffectType::GaussianFilter);
      }
    }

    if (gaussianFilter) {
      ImGui::Indent();
      ImGui::SliderInt("Gaussian K", &gaussianFilterK_, 1, 10);
      ImGui::SliderFloat("Gaussian Sigma", &gaussianSigma_, 0.1f, 10.0f);
      ImGui::Unindent();
    }

    bool depthBasedOutline = HasEffect(PostEffectType::DepthBasedOutline);
    if (ImGui::Checkbox("DepthBasedOutline", &depthBasedOutline)) {
      if (depthBasedOutline) {
        AddEffect(PostEffectType::DepthBasedOutline);
      } else {
        RemoveEffect(PostEffectType::DepthBasedOutline);
      }
    }

    if (depthBasedOutline) {
      ImGui::Indent();
      bool changed = false;
      if (ImGui::ColorEdit4("Outline Color", outlineColor_)) changed = true;
      if (ImGui::SliderFloat("Outline Weight", &outlineWeight_, 0.0f, 20.0f)) changed = true;
      if (ImGui::SliderFloat("Outline Thickness", &outlineThickness_, 0.1f, 10.0f)) changed = true;
      
      const char* modes[] = { "Both (両側)", "Outside (外側)", "Inside (内側)" };
      if (ImGui::Combo("Outline Mode", &outlineMode_, modes, 3)) changed = true;

      if (changed) {
        SetOutlineColor(outlineColor_);
        SetOutlineWeight(outlineWeight_);
        SetOutlineThickness(outlineThickness_);
        SetOutlineMode(outlineMode_);
      }
      ImGui::Unindent();
    }

    bool radialBlur = HasEffect(PostEffectType::RadialBlur);
    if (ImGui::Checkbox("RadialBlur", &radialBlur)) {
      if (radialBlur) {
        AddEffect(PostEffectType::RadialBlur);
      } else {
        RemoveEffect(PostEffectType::RadialBlur);
      }
    }

    if (radialBlur) {
      ImGui::Indent();
      ImGui::SliderFloat2("Center", &radialBlurCenter_.x, 0.0f, 1.0f);
      ImGui::SliderFloat("BlurWidth", &radialBlurWidth_, -0.1f, 0.1f);
      ImGui::SliderInt("Samples", &radialBlurSamples_, 1, 50);
      ImGui::Unindent();
    }

    bool dissolve = HasEffect(PostEffectType::Dissolve);
    if (ImGui::Checkbox("Dissolve", &dissolve)) {
      if (dissolve) {
        AddEffect(PostEffectType::Dissolve);
      } else {
        RemoveEffect(PostEffectType::Dissolve);
      }
    }

    if (dissolve) {
      ImGui::Indent();
      bool changed = false;
      if (ImGui::ColorEdit3("EdgeColor", dissolveEdgeColor_)) changed = true;
      if (ImGui::ColorEdit4("BaseColor", dissolveBaseColor_)) changed = true;
      if (ImGui::SliderFloat("Threshold", &dissolveThreshold_, 0.0f, 1.0f)) changed = true;
      if (ImGui::SliderFloat("EdgeRange", &dissolveEdgeRange_, 0.001f, 0.2f)) changed = true;

      if (changed) {
        SetDissolveEdgeColor(dissolveEdgeColor_[0], dissolveEdgeColor_[1], dissolveEdgeColor_[2]);
        SetDissolveBaseColor(dissolveBaseColor_[0], dissolveBaseColor_[1], dissolveBaseColor_[2], dissolveBaseColor_[3]);
        SetDissolveThreshold(dissolveThreshold_);
        SetDissolveEdgeRange(dissolveEdgeRange_);
      }

      // ノイズ選択コンボボックス
      if (!dissolveNoiseTextures_.empty()) {
        const char* currentName = dissolveNoiseTextures_[dissolveNoiseIndex_].name.c_str();
        if (ImGui::BeginCombo("Noise", currentName)) {
          for (int i = 0; i < static_cast<int>(dissolveNoiseTextures_.size()); ++i) {
            bool selected = (i == dissolveNoiseIndex_);
            if (ImGui::Selectable(dissolveNoiseTextures_[i].name.c_str(), selected)) {
              dissolveNoiseIndex_ = i;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
      }
      ImGui::Unindent();
    }

    bool randomNoise = HasEffect(PostEffectType::RandomNoise);
    if (ImGui::Checkbox("RandomNoise", &randomNoise)) {
      if (randomNoise) {
        AddEffect(PostEffectType::RandomNoise);
      } else {
        RemoveEffect(PostEffectType::RandomNoise);
      }
    }
    if (randomNoise) {
      ImGui::Indent();
      ImGui::Text("Time: %.3f", randomTime_);
      if (ImGui::SliderFloat("Intensity", &randomIntensity_, 0.0f, 1.0f)) {
        if (mappedRandom_) mappedRandom_->intensity = randomIntensity_;
      }
      if (ImGui::ColorEdit3("Noise Color", randomColor_)) {
        if (mappedRandom_) {
          mappedRandom_->color[0] = randomColor_[0];
          mappedRandom_->color[1] = randomColor_[1];
          mappedRandom_->color[2] = randomColor_[2];
        }
      }
      ImGui::Unindent();
    }

    // 現在のスタック表示
    const auto &effects = GetEffects();
    if (effects.empty()) {
      ImGui::TextDisabled("Active: None");
    } else {
      ImGui::Text("Active: %zu effect(s)", effects.size());
    }
  }
#endif
}
