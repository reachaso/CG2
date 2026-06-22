#include "RenderCommon.h"
#include "RenderContext.h"
#include "Mesh/PrimitiveMesh.h"
#include "Mesh/MeshGenerator.h"

namespace RC {

int GeneratePlane(float width, float height, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GeneratePlane(width, height);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Plane");
}

int GenerateBox(float width, float height, float depth, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GenerateBox(width, height, depth);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Box");
}

int GenerateSphere(float radius, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GenerateSphere(radius);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Sphere");
}

int GenerateCylinder(float radius, float height, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GenerateCylinder(radius, height);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Cylinder");
}

int GenerateCone(float radius, float height, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GenerateCone(radius, height);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Cone");
}

int GenerateTorus(float majorRadius, float minorRadius, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GenerateTorus(majorRadius, minorRadius);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Torus");
}

int GenerateCapsule(float radius, float height, int texHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GenerateCapsule(radius, height);
  return ctx.PrimitiveMeshes().Create(data, texHandle, "Capsule");
}

void DrawPrimitiveMesh(int meshHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;

  auto *m = ctx.PrimitiveMeshes().Get(meshHandle);
  if (!m) return;

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  BlendMode blend = ctx.CurrentBlendMode();

  auto layer = (blend == kBlendModeNone || blend == kBlendModeNormal) ? SortKey::kLayerOpaque : SortKey::kLayerTranslucent;
  const uint64_t key = SortKey::Make(layer, SortKey::HashPSO("object3d"), 0);
  ctx.PushCommand3D(key, [m, meshHandle, world, texHandle, lightAddr, blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    // デバッグシェーディングモードの判定
    bool isDebug = (shadingMode == ViewShadingMode::FaceOrientation ||
                    shadingMode == ViewShadingMode::RandomColor ||
                    shadingMode == ViewShadingMode::SolidShading);

    if (isDebug) {
      // デバッグモード: 専用 PSO で描画（BlendMode=None固定）
      auto savedBlend = ctx.CurrentBlendMode();
      ctx.SetBlendMode(kBlendModeNone);

      std::string_view prefix = "object3d";
      switch (shadingMode) {
      case ViewShadingMode::FaceOrientation: prefix = "object3d_faceori"; break;
      case ViewShadingMode::RandomColor:     prefix = "object3d_randcolor"; break;
      case ViewShadingMode::SolidShading:    prefix = "object3d_solid"; break;
      default: break;
      }

      if (ctx.BindPipeline(prefix)) {
        ctx.BindCameraCB();
        ctx.BindAllLightCBs();
        ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
        m->Draw(cl, world);
      }

      ctx.SetBlendMode(savedBlend);
    } else {
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (ctx.BindPipeline("object3d")) {
          ctx.BindCameraCB();
          cl->SetGraphicsRootConstantBufferView(3, lightAddr);
          ctx.BindAllLightCBs();

          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        if (ctx.BindPipeline("object3d_wire")) {
          ctx.BindCameraCB();
          cl->SetGraphicsRootConstantBufferView(3, lightAddr); // object3d用レイアウトなので必要
          ctx.BindAllLightCBs();

          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  }, "PrimitiveMesh", meshHandle);
}

void DrawPrimitiveMeshWater(int meshHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;

  auto *m = ctx.PrimitiveMeshes().Get(meshHandle);
  if (!m) return;

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  
  // Water layer (same as glass: translucent layer)
  const uint64_t key = SortKey::Make(SortKey::kLayerGlass, SortKey::HashPSO("object3d_water"), 0);

  ctx.PushCommand3D(key, [m, meshHandle, world, texHandle, lightAddr](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(kBlendModePremultiplied);

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    bool isDebug = (shadingMode == ViewShadingMode::FaceOrientation ||
                    shadingMode == ViewShadingMode::RandomColor ||
                    shadingMode == ViewShadingMode::SolidShading);

    if (isDebug) {
      auto savedBlend = ctx.CurrentBlendMode();
      ctx.SetBlendMode(kBlendModeNone);

      std::string_view prefix = "object3d";
      switch (shadingMode) {
      case ViewShadingMode::FaceOrientation: prefix = "object3d_faceori"; break;
      case ViewShadingMode::RandomColor:     prefix = "object3d_randcolor"; break;
      case ViewShadingMode::SolidShading:    prefix = "object3d_solid"; break;
      default: break;
      }

      if (ctx.BindPipeline(prefix)) {
        ctx.BindCameraCB();
        ctx.BindAllLightCBs();
        ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
        m->Draw(cl, world);
      }
      ctx.SetBlendMode(savedBlend);
    } else {
      // 2-pass water: back face first, then front face
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (ctx.BindPipeline("object3d_water_front")) {
          ctx.BindCameraCB();
          ctx.BindAllLightCBs();
          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
        if (ctx.BindPipeline("object3d_water")) {
          ctx.BindCameraCB();
          ctx.BindAllLightCBs();
          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        if (ctx.BindPipeline("object3d_wire")) {
          ctx.BindCameraCB();
          ctx.BindAllLightCBs();
          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  }, "PrimitiveMesh(Water)", meshHandle);
}

void DrawPrimitiveMeshWaterColumn(int meshHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;

  auto *m = ctx.PrimitiveMeshes().Get(meshHandle);
  if (!m) return;

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  
  // Water column layer (same as glass: translucent layer)
  const uint64_t key = SortKey::Make(SortKey::kLayerGlass, SortKey::HashPSO("object3d_watercolumn"), 0);

  ctx.PushCommand3D(key, [m, meshHandle, world, texHandle, lightAddr](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(kBlendModePremultiplied);

    ViewShadingMode shadingMode = ctx.GetViewShadingMode();

    bool isDebug = (shadingMode == ViewShadingMode::FaceOrientation ||
                    shadingMode == ViewShadingMode::RandomColor ||
                    shadingMode == ViewShadingMode::SolidShading);

    if (isDebug) {
      auto savedBlend = ctx.CurrentBlendMode();
      ctx.SetBlendMode(kBlendModeNone);

      std::string_view prefix = "object3d";
      switch (shadingMode) {
      case ViewShadingMode::FaceOrientation: prefix = "object3d_faceori"; break;
      case ViewShadingMode::RandomColor:     prefix = "object3d_randcolor"; break;
      case ViewShadingMode::SolidShading:    prefix = "object3d_solid"; break;
      default: break;
      }

      if (ctx.BindPipeline(prefix)) {
        ctx.BindCameraCB();
        ctx.BindAllLightCBs();
        ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
        m->Draw(cl, world);
      }
      ctx.SetBlendMode(savedBlend);
    } else {
      // 2-pass water column: back face first, then front face
      if (shadingMode != ViewShadingMode::Wireframe) {
        if (ctx.BindPipeline("object3d_watercolumn_front")) {
          ctx.BindCameraCB();
          ctx.BindAllLightCBs();
          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
        if (ctx.BindPipeline("object3d_watercolumn")) {
          ctx.BindCameraCB();
          ctx.BindAllLightCBs();
          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
      }
      if (shadingMode == ViewShadingMode::Wireframe || shadingMode == ViewShadingMode::SolidWireframe) {
        if (ctx.BindPipeline("object3d_wire")) {
          ctx.BindCameraCB();
          ctx.BindAllLightCBs();
          ctx.PrimitiveMeshes().ApplyTexture(meshHandle, texHandle);
          m->Draw(cl, world);
        }
      }
    }
    ctx.SetBlendMode(prevBlend);
  }, "PrimitiveMesh(WaterColumn)", meshHandle);
}

void UnloadPrimitiveMesh(int meshHandle) {
  GetRenderContext().PrimitiveMeshes().Unload(meshHandle);
}

Transform *GetPrimitiveMeshTransformPtr(int meshHandle) {
  return GetRenderContext().PrimitiveMeshes().GetTransformPtr(meshHandle);
}

void DrawPrimitiveMeshImGui(int meshHandle, const char *name) {
  GetRenderContext().PrimitiveMeshes().DrawImGui(meshHandle, name);
}

void SetPrimitiveMeshEnvironmentCoefficient(int meshHandle, float coeff) {
  auto *m = GetRenderContext().PrimitiveMeshes().Get(meshHandle);
  if (!m) return;
  if (auto* mat = m->Mat()) {
    mat->environmentCoefficient = coeff;
  }
}

Material *GetPrimitiveMeshMaterialPtr(int meshHandle) {
  auto *m = GetRenderContext().PrimitiveMeshes().Get(meshHandle);
  if (!m) return nullptr;
  return m->Mat();
}

void SetPrimitiveMeshNormalMap(int meshHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.PrimitiveMeshes().Get(meshHandle);
  if (!m) return;
  if (texHandle >= 0) {
    m->SetNormalMap(ctx.Textures().GetSrv(texHandle));
  } else {
    m->SetNormalMap(D3D12_GPU_DESCRIPTOR_HANDLE{0});
  }
}

void SetPrimitiveMeshRoughnessMap(int meshHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.PrimitiveMeshes().Get(meshHandle);
  if (!m) return;
  if (texHandle >= 0) {
    m->SetRoughnessMap(ctx.Textures().GetSrv(texHandle));
  } else {
    m->SetRoughnessMap(D3D12_GPU_DESCRIPTOR_HANDLE{0});
  }
}

} // namespace RC
