// ============================================================================
// RenderModel.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Model 描画 API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Model/ModelObject.h"

namespace RC {

// ============================================================================
// ライト適用の共通ヘルパー（file-scope）
// ============================================================================
namespace {

/// ディレクショナルライトの CB アドレスをモデルに適用し、
/// ライティングモード/shininess をマテリアルに同期する。
/// @return lightAddr (0 = デフォルト使用)
static D3D12_GPU_VIRTUAL_ADDRESS ApplyDirLight_(RenderContext &ctx,
                                                ModelObject *m) {
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      ctx.DirLights().GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = ctx.DirLights().GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }
  }
  return lightAddr;
}

/// ガラス用ライト適用（shininess は上書きしない）
static void ApplyDirLightGlass_(RenderContext &ctx, ModelObject *m) {
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      ctx.DirLights().GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = ctx.DirLights().GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
      }
    }
  }
}

/// 標準的な 3D バインド（Pipeline + Camera + Lights）
static bool BindStandard3D_(RenderContext &ctx, std::string_view prefix) {
  if (!ctx.BindPipeline(prefix)) {
    return false;
  }
  ctx.BindCameraCB();
  ctx.BindAllLightCBs();
  return true;
}

/// テクスチャのオーバーライド
static void ApplyTexture_(RenderContext &ctx, ModelObject *m, int texHandle) {
  if (texHandle >= 0) {
    m->SetTexture(ctx.Textures().GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }
}

// ============================================================================
// ViewShadingMode ヘルパー
// ============================================================================

/// デバッグ用のプロシージャルシェーダーモードか判定
static bool IsDebugShadingMode_(ViewShadingMode mode) {
  return mode == ViewShadingMode::FaceOrientation ||
         mode == ViewShadingMode::RandomColor ||
         mode == ViewShadingMode::SolidShading;
}

/// ViewShadingMode に応じた単体描画用 Pipeline prefix を返す
static std::string_view ResolveSinglePrefix_(ViewShadingMode mode, bool skinned) {
  switch (mode) {
  case ViewShadingMode::FaceOrientation: return skinned ? "object3d_faceori_skin" : "object3d_faceori";
  case ViewShadingMode::RandomColor:     return skinned ? "object3d_randcolor_skin" : "object3d_randcolor";
  case ViewShadingMode::SolidShading:    return skinned ? "object3d_solid_skin" : "object3d_solid";
  default:                               return skinned ? "object3d_skin" : "object3d";
  }
}

/// ViewShadingMode に応じたインスタンシング描画用 Pipeline prefix を返す
static std::string_view ResolveInstPrefix_(ViewShadingMode mode) {
  switch (mode) {
  case ViewShadingMode::FaceOrientation: return "object3d_faceori_inst";
  case ViewShadingMode::RandomColor:     return "object3d_randcolor_inst";
  case ViewShadingMode::SolidShading:    return "object3d_solid_inst";
  default:                               return "object3d_inst";
  }
}

/// デバッグモード時の描画実行（単体）
/// デバッグシェーダーは BlendMode=None 固定、ワイヤーフレーム無し
static void DrawDebugSingle_(RenderContext &ctx, ModelObject *m,
                              ID3D12GraphicsCommandList *cl,
                              const Matrix4x4 &world,
                              ViewShadingMode mode,
                              bool skinned) {
  auto prevBlend = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModeNone);

  if (BindStandard3D_(ctx, ResolveSinglePrefix_(mode, skinned))) {
    m->Draw(cl, world, ctx.CurrentFrame());
  }

  ctx.SetBlendMode(prevBlend);
}

/// デバッグモード時の描画実行（インスタンシング）
static void DrawDebugInst_(RenderContext &ctx, ModelObject *m,
                            ID3D12GraphicsCommandList *cl,
                            const std::vector<Transform> &instances,
                            ViewShadingMode mode) {
  auto prevBlend = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModeNone);

  if (BindStandard3D_(ctx, ResolveInstPrefix_(mode))) {
    m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
  }

  ctx.SetBlendMode(prevBlend);
}

/// デバッグモード時の描画実行（インスタンシング + カラー）
static void DrawDebugInstColored_(RenderContext &ctx, ModelObject *m,
                                   ID3D12GraphicsCommandList *cl,
                                   const std::vector<Transform> &instances,
                                   const Vector4 &color,
                                   ViewShadingMode mode) {
  auto prevBlend = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModeNone);

  if (BindStandard3D_(ctx, ResolveInstPrefix_(mode))) {
    m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
  }

  ctx.SetBlendMode(prevBlend);
}

} // namespace

// ============================================================================
// Load / Unload
// ============================================================================

int LoadModel(const std::string &path) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }
  return ctx.Models().Load(path);
}

void UnloadModel(int modelHandle) {
  GetRenderContext().Models().Unload(modelHandle);
}

// ============================================================================
// Draw (基本)
// ============================================================================

void DrawModel(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  BlendMode blend = ctx.CurrentBlendMode();

  // ソートキー構築（Opaqueレイヤー + object3d PSO）
  const uint64_t key = SortKey::Make(SortKey::kLayerOpaque,
                                     SortKey::HashPSO("object3d"), 0);

  ctx.PushCommand3D(key, [m, world, texHandle, lightAddr, blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    ApplyTexture_(ctx, m, texHandle);
    m->SetExternalLightCBAddress(lightAddr);
    if (lightAddr != 0) {
      ApplyDirLight_(ctx, m);
    }
    m->Update(ctx.View(), ctx.Proj());

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    // スキニングモデルかどうかでパイプラインを分岐
    const bool skinned = m->HasSkinData();
    // CS スキニングが有効なら通常パイプライン（object3d）で描画
    const bool useCSSkinning = skinned && m->Resource().HasCSSkinning();
    const std::string_view pipelinePrefix = (skinned && !useCSSkinning) ? "object3d_skin" : "object3d";

    // CS スキニング: Graphics パイプラインバインド前に Dispatch を実行
    if (useCSSkinning && !m->Resource().IsSkinningDispatched()) {
      m->Resource().DispatchSkinning(cl, m->GetSkinMatrices(), ctx.CurrentFrame());
    }

    if (IsDebugShadingMode_(shadingMode)) {
      DrawDebugSingle_(ctx, m, cl, world, shadingMode, skinned && !useCSSkinning);
    } else {
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (BindStandard3D_(ctx, pipelinePrefix)) {
          m->Draw(cl, world, ctx.CurrentFrame());
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        const std::string_view wirePipeline = (skinned && !useCSSkinning) ? "object3d_wire_skin" : "object3d_wire";
        if (BindStandard3D_(ctx, wirePipeline)) {
          m->Draw(cl, world, ctx.CurrentFrame());
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  });
}

void DrawModel(int modelHandle) { DrawModel(modelHandle, -1); }

void DrawModelNoCull(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  BlendMode blend = ctx.CurrentBlendMode();

  const uint64_t key = SortKey::Make(SortKey::kLayerOpaque,
                                     SortKey::HashPSO("object3d_nocull"), 0);

  ctx.PushCommand3D(key, [m, world, texHandle, lightAddr, blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    ApplyTexture_(ctx, m, texHandle);
    m->SetExternalLightCBAddress(lightAddr);
    if (lightAddr != 0) {
      ApplyDirLight_(ctx, m);
    }
    m->Update(ctx.View(), ctx.Proj());

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();
    const bool skinned = m->HasSkinData();
    const bool useCSSkinning = skinned && m->Resource().HasCSSkinning();

    // CS スキニング: Graphics パイプラインバインド前に Dispatch を実行
    if (useCSSkinning && !m->Resource().IsSkinningDispatched()) {
      m->Resource().DispatchSkinning(cl, m->GetSkinMatrices(), ctx.CurrentFrame());
    }

    if (IsDebugShadingMode_(shadingMode)) {
      DrawDebugSingle_(ctx, m, cl, world, shadingMode, skinned && !useCSSkinning);
    } else {
      const std::string_view pipeline = (skinned && !useCSSkinning) ? "object3d_skin_nocull" : "object3d_nocull";
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (BindStandard3D_(ctx, pipeline)) {
          m->Draw(cl, world, ctx.CurrentFrame());
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        const std::string_view wirePipeline = (skinned && !useCSSkinning) ? "object3d_wire_skin" : "object3d_wire";
        if (BindStandard3D_(ctx, wirePipeline)) {
          m->Draw(cl, world, ctx.CurrentFrame());
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  });
}

// ============================================================================
// Batch
// ============================================================================

void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  BlendMode blend = ctx.CurrentBlendMode();

  const uint64_t key = SortKey::Make(SortKey::kLayerOpaque,
                                     SortKey::HashPSO("object3d_inst"), 0);

  ctx.PushCommand3D(key, [m, instances, texHandle, lightAddr, blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    ApplyTexture_(ctx, m, texHandle);
    m->SetExternalLightCBAddress(lightAddr);
    if (lightAddr != 0) {
      ApplyDirLight_(ctx, m);
    }

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    if (IsDebugShadingMode_(shadingMode)) {
      DrawDebugInst_(ctx, m, cl, instances, shadingMode);
    } else {
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (BindStandard3D_(ctx, "object3d_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        if (BindStandard3D_(ctx, "object3d_wire_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  });
}

void DrawModelBatchColored(int modelHandle,
                           const std::vector<Transform> &instances,
                           const Vector4 &color, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  BlendMode blend = ctx.CurrentBlendMode();

  const uint64_t key = SortKey::Make(SortKey::kLayerOpaque,
                                     SortKey::HashPSO("object3d_inst"), 0);

  ctx.PushCommand3D(key, [m, instances, color, texHandle,
                     lightAddr, blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    ApplyTexture_(ctx, m, texHandle);
    m->SetExternalLightCBAddress(lightAddr);
    if (lightAddr != 0) {
      ApplyDirLight_(ctx, m);
    }

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    if (IsDebugShadingMode_(shadingMode)) {
      DrawDebugInstColored_(ctx, m, cl, instances, color, shadingMode);
    } else {
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (BindStandard3D_(ctx, "object3d_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        if (BindStandard3D_(ctx, "object3d_wire_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  });
}

// ============================================================================
// Glass (1パス)
// ============================================================================

void DrawModelGlass(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();

  const uint64_t key = SortKey::Make(SortKey::kLayerGlass,
                                     SortKey::HashPSO("object3d_glass"), 0);

  ctx.PushCommand3D(key,
      [m, world, texHandle, lightAddr](ID3D12GraphicsCommandList *cl) {
        auto &ctx = GetRenderContext();
        const BlendMode saved = ctx.CurrentBlendMode();
        ctx.SetBlendMode(kBlendModePremultiplied);

        ApplyTexture_(ctx, m, texHandle);
        m->SetExternalLightCBAddress(lightAddr);
        if (lightAddr != 0) {
          ApplyDirLightGlass_(ctx, m);
        }
        m->Update(ctx.View(), ctx.Proj());

        ViewShadingMode shadingMode = ctx.GetViewShadingMode();
        const bool skinned = m->HasSkinData();

        if (IsDebugShadingMode_(shadingMode)) {
          DrawDebugSingle_(ctx, m, cl, world, shadingMode, skinned);
        } else {
          if (shadingMode != ViewShadingMode::Wireframe) {
            if (BindStandard3D_(ctx, "object3d_glass")) {
              m->Draw(cl, world, ctx.CurrentFrame());
            }
          }
          if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
            const std::string_view wirePipeline = skinned ? "object3d_wire_skin" : "object3d_wire";
            if (BindStandard3D_(ctx, wirePipeline)) {
              m->Draw(cl, world, ctx.CurrentFrame());
            }
          }
        }
        ctx.SetBlendMode(saved);
      });
}

void DrawModelGlassBatch(int modelHandle,
                         const std::vector<Transform> &instances,
                         int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();

  const uint64_t key = SortKey::Make(SortKey::kLayerGlass,
                                     SortKey::HashPSO("object3d_glass_inst"), 0);

  ctx.PushCommand3D(key,
      [m, instances, texHandle, lightAddr](ID3D12GraphicsCommandList *cl) {
        auto &ctx = GetRenderContext();
        const BlendMode saved = ctx.CurrentBlendMode();
        ctx.SetBlendMode(kBlendModePremultiplied);

        ApplyTexture_(ctx, m, texHandle);
        m->SetExternalLightCBAddress(lightAddr);
        if (lightAddr != 0) {
          ApplyDirLightGlass_(ctx, m);
        }

        ViewShadingMode shadingMode = ctx.GetViewShadingMode();

        if (IsDebugShadingMode_(shadingMode)) {
          DrawDebugInst_(ctx, m, cl, instances, shadingMode);
        } else {
          if (shadingMode != ViewShadingMode::Wireframe) {
            if (BindStandard3D_(ctx, "object3d_glass_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
            }
          }
          if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
            if (BindStandard3D_(ctx, "object3d_wire_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
            }
          }
        }
        ctx.SetBlendMode(saved);
      });
}

void DrawModelGlassBatchColored(int modelHandle,
                                const std::vector<Transform> &instances,
                                const Vector4 &color, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();

  const uint64_t key = SortKey::Make(SortKey::kLayerGlass,
                                     SortKey::HashPSO("object3d_glass_inst"), 0);

  ctx.PushCommand3D(key,
      [m, instances, color, texHandle,
       lightAddr](ID3D12GraphicsCommandList *cl) {
        auto &ctx = GetRenderContext();
        const BlendMode saved = ctx.CurrentBlendMode();
        ctx.SetBlendMode(kBlendModePremultiplied);

        ApplyTexture_(ctx, m, texHandle);
        m->SetExternalLightCBAddress(lightAddr);
        if (lightAddr != 0) {
          ApplyDirLightGlass_(ctx, m);
        }

        ViewShadingMode shadingMode = ctx.GetViewShadingMode();

        if (IsDebugShadingMode_(shadingMode)) {
          DrawDebugInstColored_(ctx, m, cl, instances, color, shadingMode);
        } else {
          if (shadingMode != ViewShadingMode::Wireframe) {
            if (BindStandard3D_(ctx, "object3d_glass_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
            }
          }
          if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
            if (BindStandard3D_(ctx, "object3d_wire_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
            }
          }
        }
        ctx.SetBlendMode(saved);
      });
}


// ============================================================================
// Glass (2パス: 背面→表面)
// ※ 2パスは同一パケット内で PSO を切替える必要があるため、
//    ラムダ方式（PushCommand3D）のまま残す。
// ============================================================================

void DrawModelGlassTwoPass(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();


  const uint64_t key = SortKey::Make(SortKey::kLayerGlass,
                                     SortKey::HashPSO("object3d_glass"), 0);

  ctx.PushCommand3D(key,
      [m, world, texHandle, lightAddr](ID3D12GraphicsCommandList *cl) {
        auto &ctx = GetRenderContext();
        const BlendMode saved = ctx.CurrentBlendMode();
        ctx.SetBlendMode(kBlendModePremultiplied);

        ApplyTexture_(ctx, m, texHandle);
        m->SetExternalLightCBAddress(lightAddr);
        if (lightAddr != 0) {
          ApplyDirLightGlass_(ctx, m);
        }
        m->Update(ctx.View(), ctx.Proj());

        ViewShadingMode shadingMode = ctx.GetViewShadingMode();
        const bool skinned = m->HasSkinData();

        if (IsDebugShadingMode_(shadingMode)) {
          DrawDebugSingle_(ctx, m, cl, world, shadingMode, skinned);
        } else {
          // 1) 背面（内側） - Solidのみ
          if (shadingMode != ViewShadingMode::Wireframe) {
            if (BindStandard3D_(ctx, "object3d_glass_front")) {
              m->Draw(cl, world, ctx.CurrentFrame());
            }
          }
          // 2) 表面（外側） - SolidWireframeならワイヤーフレームも重ねる
          if (shadingMode != ViewShadingMode::Wireframe) {
            if (BindStandard3D_(ctx, "object3d_glass")) {
              m->Draw(cl, world, ctx.CurrentFrame());
            }
          }
          if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
            const std::string_view wirePipeline = skinned ? "object3d_wire_skin" : "object3d_wire";
            if (BindStandard3D_(ctx, wirePipeline)) {
              m->Draw(cl, world, ctx.CurrentFrame());
            }
          }
        }

        ctx.SetBlendMode(saved);
      });
}

void DrawModelGlassTwoPassBatch(int modelHandle,
                                const std::vector<Transform> &instances,
                                int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();

  const uint64_t key = SortKey::Make(SortKey::kLayerGlass,
                                     SortKey::HashPSO("object3d_glass"), 0);

  ctx.PushCommand3D(key,
      [m, instances, texHandle, lightAddr](ID3D12GraphicsCommandList *cl) {
        auto &ctx = GetRenderContext();
        const BlendMode saved = ctx.CurrentBlendMode();
        ctx.SetBlendMode(kBlendModePremultiplied);

        ApplyTexture_(ctx, m, texHandle);
        m->SetExternalLightCBAddress(lightAddr);
        if (lightAddr != 0) {
          ApplyDirLightGlass_(ctx, m);
        }

        ViewShadingMode shadingMode = ctx.GetViewShadingMode();

        if (IsDebugShadingMode_(shadingMode)) {
          DrawDebugInst_(ctx, m, cl, instances, shadingMode);
        } else {
          if (shadingMode != ViewShadingMode::Wireframe) {
            if (BindStandard3D_(ctx, "object3d_glass_front_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
            }
            if (BindStandard3D_(ctx, "object3d_glass_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
            }
          }
          if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
            if (BindStandard3D_(ctx, "object3d_wire_inst")) {
              m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, ctx.CurrentFrame());
            }
          }
        }

        ctx.SetBlendMode(saved);
      });
}

void DrawModelGlassTwoPassBatchColored(int modelHandle,
                                       const std::vector<Transform> &instances,
                                       const Vector4 &color, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();

  const uint64_t key = SortKey::Make(SortKey::kLayerGlass,
                                     SortKey::HashPSO("object3d_glass"), 0);

  ctx.PushCommand3D(key, [m, instances, color, texHandle,
                     lightAddr](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    const BlendMode saved = ctx.CurrentBlendMode();
    ctx.SetBlendMode(kBlendModePremultiplied);

    ApplyTexture_(ctx, m, texHandle);
    m->SetExternalLightCBAddress(lightAddr);
    if (lightAddr != 0) {
      ApplyDirLightGlass_(ctx, m);
    }

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    if (IsDebugShadingMode_(shadingMode)) {
      DrawDebugInstColored_(ctx, m, cl, instances, color, shadingMode);
    } else {
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (BindStandard3D_(ctx, "object3d_glass_front_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
        }
        if (BindStandard3D_(ctx, "object3d_glass_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        if (BindStandard3D_(ctx, "object3d_wire_inst")) {
          m->DrawBatch(cl, ctx.View(), ctx.Proj(), instances, color, ctx.CurrentFrame());
        }
      }
    }

    ctx.SetBlendMode(saved);
  });
}


// ============================================================================
// ImGui / Transform / Color / LightingMode / Mesh / Cursor
// ============================================================================

void DrawImGui3D(int modelHandle, const char *name) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const bool showLightingUi = (ctx.DirLights().GetActiveCBAddress() == 0);
  m->DrawImGui(name, showLightingUi);
}

Transform *GetModelTransformPtr(int modelHandle) {
  return GetRenderContext().Models().GetTransformPtr(modelHandle);
}

void SetModelColor(int modelHandle, const Vector4 &color) {
  GetRenderContext().Models().SetColor(modelHandle, color);
}

void SetModelLightingMode(int modelHandle, LightingMode m) {
  GetRenderContext().Models().SetLightingMode(modelHandle, m);
}

void SetModelMesh(int modelHandle, const std::string &path) {
  GetRenderContext().Models().SetMesh(modelHandle, path);
}

void ResetCursor(int modelHandle) {
  GetRenderContext().Models().ResetCursor(modelHandle);
}

void SetModelEnvironmentCoefficient(int modelHandle, float coeff) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) return;
  m->SetEnvironmentCoefficient(coeff);
}

void SetModelNormalMap(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) return;
  if (texHandle >= 0) {
    m->SetNormalMap(ctx.Textures().GetSrv(texHandle));
  } else {
    m->SetNormalMap(D3D12_GPU_DESCRIPTOR_HANDLE{0});
  }
}

void SetModelRoughnessMap(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) return;
  if (texHandle >= 0) {
    m->SetRoughnessMap(ctx.Textures().GetSrv(texHandle));
  } else {
    m->SetRoughnessMap(D3D12_GPU_DESCRIPTOR_HANDLE{0});
  }
}

void AttachModelAnimation(int modelHandle) {
  GetRenderContext().Models().AttachAnimation(modelHandle);
}

void AttachModelAnimation(int modelHandle, const std::string& filePath) {
  GetRenderContext().Models().AttachAnimation(modelHandle, filePath);
}

void UpdateModelAnimation(int modelHandle, float dt) {
  if (dt < 0.0f) {
    if (auto* ctx = GetRenderContext().Ctx()) {
      dt = ctx->deltaTime;
    } else {
      dt = 1.0f / 60.0f; // fallback
    }
  }
  GetRenderContext().Models().UpdateAnimation(modelHandle, dt);
}

void DrawModelSkeleton(int modelHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) return;
  m->DrawSkeleton();
}

bool HasModelSkinData(int modelHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) return false;
  return m->HasSkinData();
}

Material *GetModelMaterialPtr(int modelHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) return nullptr;
  return m->Mat();
}

} // namespace RC
