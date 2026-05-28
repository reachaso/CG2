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

  ctx.PushCommand3D([m, meshHandle, world, texHandle, lightAddr, blend](ID3D12GraphicsCommandList *cl) {
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
  });
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

} // namespace RC
