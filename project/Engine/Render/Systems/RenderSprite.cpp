// ============================================================================
// RenderSprite.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Sprite 描画 API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Scene.h"

namespace RC {

int LoadSprite(const std::string &path, SceneContext &ctx, bool srgb) {
  auto &rc = GetRenderContext();
  if (!rc.IsInitialized()) {
    return -1;
  }

  const float screenW = static_cast<float>(ctx.app->width);
  const float screenH = static_cast<float>(ctx.app->height);
  return rc.Sprites().Load(path, screenW, screenH, srgb);
}

void DrawSprite(int spriteHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("sprite")) {
    return;
  }

  ctx.AddCommandHistory("Sprite", spriteHandle);
  ctx.Sprites().Draw(spriteHandle, ctx.CL());
}

void DrawSpriteRect(int spriteHandle, float srcX, float srcY, float srcW,
                    float srcH, float texW, float texH, float insetPx) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("sprite")) {
    return;
  }

  ctx.AddCommandHistory("SpriteRect", spriteHandle);
  ctx.Sprites().DrawRect(spriteHandle, srcX, srcY, srcW, srcH, texW, texH,
                         insetPx, ctx.CL());
}

void DrawSpriteRectUV(int spriteHandle, float u0, float v0, float u1,
                      float v1) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("sprite")) {
    return;
  }

  ctx.AddCommandHistory("SpriteRectUV", spriteHandle);
  ctx.Sprites().DrawRectUV(spriteHandle, u0, v0, u1, v1, ctx.CL());
}

void SetSpriteTransform(int spriteHandle, const Transform &t) {
  GetRenderContext().Sprites().SetTransform(spriteHandle, t);
}

void SetSpriteColor(int spriteHandle, const Vector4 &color) {
  GetRenderContext().Sprites().SetColor(spriteHandle, color);
}

void UnloadSprite(int spriteHandle) {
  GetRenderContext().Sprites().Unload(spriteHandle);
}

void SetSpriteScreenSize(int spriteHandle, float w, float h) {
  GetRenderContext().Sprites().SetSize(spriteHandle, w, h);
}

void DrawImGui2D(int spriteHandle, const char *name) {
  GetRenderContext().Sprites().DrawImGui(spriteHandle, name);
}

} // namespace RC
