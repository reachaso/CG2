// ============================================================================
// RenderCommon.cpp
// ----------------------------------------------------------------------------
// RenderCommon は「Scene から呼べる描画ファサード。」
//
// 責務分割後、このファイルに残るもの:
//   - RenderContext の唯一のインスタンス管理
//   - Init / Term
//   - SetCamera
//   - PreDraw3D / PreDraw2D
//   - Texture (LoadTex / GetSrv)
//   - BlendMode (Set / Get)
//   - IsInitialized / GetDevice
//
// Model / Sprite / Sphere / Light / Primitive の実装は
// 各 Render*.cpp で RC::名前空間の関数として定義される。
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Dx12/Dx12Core.h"
#include "PipelineManager.h"
#include "Primitive/Primitive2D.h"
#include "Primitive/Primitive3D.h"
#include "Graphics/PostProcess/PostProcess.h"
#include "Scene.h"
#include "imgui/imgui.h"
#include "Common/EngineConfig.h" // Added for RC_ENABLE_IMGUI
#include "Common/Log/Log.h"

#include <string_view>
#include <format>

namespace RC {

// ============================================================================
// Init / Term
// ============================================================================

void Init(SceneContext &ctx) { RenderContext::GetInstance().Init(ctx); }

void Term() { RenderContext::GetInstance().Term(); }

// ============================================================================
// Camera
// ============================================================================

void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos) {
  RenderContext::GetInstance().SetCamera(view, proj, camWorldPos);
}

// ============================================================================
// 3D Pass
// ============================================================================

void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RenderContext::GetInstance().PreDraw3D(ctx, cl);
}

// ============================================================================
// 2D Pass
// ============================================================================

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RenderContext::GetInstance().PreDraw2D(ctx, cl);
}

// ============================================================================
// Texture
// ============================================================================

int LoadTex(const std::string &path, bool srgb) {
  return RenderContext::GetInstance().LoadTex(path, srgb);
}

D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle) {
  return RenderContext::GetInstance().GetSrv(texHandle);
}

// ============================================================================
// State
// ============================================================================

bool IsInitialized() { return RenderContext::GetInstance().IsInitialized(); }

ID3D12Device *GetDevice() { return RenderContext::GetInstance().Device(); }

void SetBlendMode(BlendMode blendMode) {
  RenderContext::GetInstance().SetBlendMode(blendMode);
}

BlendMode GetBlendMode() {
  return RenderContext::GetInstance().CurrentBlendMode();
}

void SetViewShadingMode(ViewShadingMode mode) {
  ViewShadingMode current = RenderContext::GetInstance().GetViewShadingMode();
  if (current != mode) {
    RenderContext::GetInstance().SetViewShadingMode(mode);
    const char *modeStr = "Solid";
    if (mode == ViewShadingMode::Wireframe) modeStr = "Wireframe";
    else if (mode == ViewShadingMode::SolidWireframe) modeStr = "Solid + Wireframe";
    else if (mode == ViewShadingMode::FaceOrientation) modeStr = "Face Orientation";
    else if (mode == ViewShadingMode::RandomColor) modeStr = "Random Color";
    else if (mode == ViewShadingMode::SolidShading) modeStr = "Solid Shading (Lambert)";
    Log::Print(std::format("[Render] View Shading Mode changed to: {}", modeStr));
  }
}

ViewShadingMode GetViewShadingMode() {
  return RenderContext::GetInstance().GetViewShadingMode();
}

#if RC_ENABLE_IMGUI
static int s_ShadingIconHandles[6] = { -1, -1, -1, -1, -1, -1 };

void DrawViewShadingModeImGui(const char *label) {
  if (s_ShadingIconHandles[0] == -1) {
    s_ShadingIconHandles[0] = LoadTex("Resources/icons/solid.png");
    s_ShadingIconHandles[1] = LoadTex("Resources/icons/wireframe.png");
    s_ShadingIconHandles[2] = LoadTex("Resources/icons/solid_wire.png");
    s_ShadingIconHandles[3] = LoadTex("Resources/icons/face.png");
    s_ShadingIconHandles[4] = LoadTex("Resources/icons/random.png");
    s_ShadingIconHandles[5] = LoadTex("Resources/icons/lambert.png");
  }

  int current = static_cast<int>(GetViewShadingMode());
  
  const char *tooltips[] = {
    "Solid",
    "Wireframe",
    "Solid + Wireframe",
    "Face Orientation",
    "Random Color",
    "Solid Shading (Lambert)"
  };

  if (label && label[0] != '\0') {
    ImGui::Text("%s", label);
    ImGui::SameLine();
  }
  
  // ボタン間の隙間を少しあける
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
  // ボタン内の余白を固定(上下左右2px)して、ボタン全体の高さを24pxに固定する
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
  
  for (int i = 0; i < 6; i++) {
    if (i > 0) ImGui::SameLine();
    
    bool is_selected = (current == i);
    
    ImVec4 bgCol = ImVec4(0, 0, 0, 0);

    // 選択中のボタンは色をハイライト
    if (is_selected) {
      bgCol = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    } else {
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
    }
    
    char str_id[32];
    snprintf(str_id, sizeof(str_id), "##ShdBtn%d", i);

    ImTextureID tex_id = 0;
    if (s_ShadingIconHandles[i] != -1) {
      tex_id = (ImTextureID)GetSrv(s_ShadingIconHandles[i]).ptr;
    }

    if (tex_id) {
        // 画像サイズを少し小さくして余白を持たせる
        // 引数: id, tex, size, uv0, uv1, bg_col
        if (ImGui::ImageButton(str_id, tex_id, ImVec2(20, 20), ImVec2(0,0), ImVec2(1,1), bgCol)) {
            SetViewShadingMode(static_cast<ViewShadingMode>(i));
        }
    } else {
        if (ImGui::Button(tooltips[i])) {
            SetViewShadingMode(static_cast<ViewShadingMode>(i));
        }
    }
    
    ImGui::PopStyleColor(2);
    
    // ホバー時に名前を表示
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltips[i]);
    }
  }
  
  ImGui::PopStyleVar(2);
}
#endif

void SetPostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetEffect(type);
  }
}

::PostEffectType GetPostEffect() {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    return pp->GetEffect();
  }
  return ::PostEffectType::None;
}

void AddPostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->AddEffect(type);
  }
}

void RemovePostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->RemoveEffect(type);
  }
}

void ClearPostEffects() {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->ClearEffects();
  }
}

bool HasPostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    return pp->HasEffect(type);
  }
  return false;
}

void SetPostProcessOutlineColor(const float color[4]) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetOutlineColor(color);
  }
}

void SetPostProcessOutlineWeight(float weight) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetOutlineWeight(weight);
  }
}

void SetPostProcessOutlineThickness(float thickness) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetOutlineThickness(thickness);
  }
}

void SetDissolveThreshold(float threshold) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetDissolveThreshold(threshold);
  }
}

void SetDissolveEdgeColor(float r, float g, float b) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetDissolveEdgeColor(r, g, b);
  }
}

void SetDissolveBaseColor(float r, float g, float b, float a) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetDissolveBaseColor(r, g, b, a);
  }
}

void SetDissolveEdgeRange(float range) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetDissolveEdgeRange(range);
  }
}

void SetDissolveNoiseIndex(int index) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetDissolveNoiseIndex(index);
  }
}

void InitDissolveNoiseTextures() {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->InitDissolveNoiseTextures();
  }
}

void SetRandomNoiseIntensity(float intensity) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetRandomNoiseIntensity(intensity);
  }
}

void SetRandomNoiseColor(float r, float g, float b) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetRandomNoiseColor(r, g, b);
  }
}

#if RC_ENABLE_IMGUI
void DrawPostEffectImGui(const char *label) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->DrawImGui(label);
  }
}
#endif

void AddLoadingTask(std::future<void> &&task) {
  RenderContext::GetInstance().AddLoadingTask(std::move(task));
}

void WaitAllLoads() { RenderContext::GetInstance().WaitAllLoads(); }

void ClearTextureLogHistory() {
  RenderContext::GetInstance().Textures().ClearLogHistory();
}

} // namespace RC
