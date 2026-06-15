// ============================================================================
// RenderSkydome.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Skydome 描画 API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"
#include "Skydome/Skydome.h"

namespace RC {

int GenerateSkydomeEx(int textureHandle, float radius, unsigned int sliceCount,
                      unsigned int stackCount) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.Device()) {
    return -1;
  }

  return ctx.Skydomes().Create(textureHandle, radius, sliceCount, stackCount);
}

int GenerateSkydome(int textureHandle) {
  return GenerateSkydomeEx(textureHandle);
}

void DrawSkydome(int skydomeHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }

  auto *s = ctx.Skydomes().Get(skydomeHandle);
  if (!s) {
    return;
  }

  // 状態キャプチャ
  Matrix4x4 world = MakeAffineMatrix(s->T().scale, s->T().rotation, s->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  BlendMode blend = ctx.CurrentBlendMode();

  const uint64_t key = SortKey::Make(SortKey::kLayerOpaque, SortKey::HashPSO("object3d"), 0);
  ctx.PushCommand3D(key, [s, skydomeHandle, world, texHandle, lightAddr,
                     blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    // Skydome は通常ライティング無しの "object3d" (またはスカイ専用PSO) を使う
    // 現状は object3d を流用
    if (ctx.BindPipeline("object3d")) {
      ctx.BindCameraCB();
      // 天球は通常ライトの影響を受けないが、一応バインド
      ctx.BindAllLightCBs();

      ctx.Skydomes().ApplyTexture(skydomeHandle, texHandle);
      s->SetExternalLightCBAddress(lightAddr);

      s->Update(ctx.View(), ctx.Proj());
      s->Draw(cl, world);
    }
    ctx.SetBlendMode(prevBlend);
  }, "Skydome", skydomeHandle);
}


void DrawSkydomeImGui(int skydomeHandle, const char *name) {
  if (auto *s = GetRenderContext().Skydomes().Get(skydomeHandle)) {
    s->DrawImGui(name);
  }
}

void UnloadSkydome(int skydomeHandle) {
  GetRenderContext().Skydomes().Unload(skydomeHandle);
}

Transform *GetSkydomeTransformPtr(int skydomeHandle) {
  return GetRenderContext().Skydomes().GetTransformPtr(skydomeHandle);
}

void SetSkydomeColor(int skydomeHandle, const Vector4 &color) {
  GetRenderContext().Skydomes().SetColor(skydomeHandle, color);
}

} // namespace RC
