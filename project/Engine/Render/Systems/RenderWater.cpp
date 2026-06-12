#include "RenderCommon.h"
#include "RenderContext.h"
#include "Mesh/PrimitiveMesh.h"
#include "Mesh/MeshGenerator.h"
#include "function/function.h"
#include "RenderInteractiveWater.h"

namespace RC {

// ============================================================================
// 水面パラメータ用 定数バッファ (GPU b6)
// ============================================================================

// HLSL の cbuffer WaterParams と完全に対応するレイアウト
struct WaterParamsCB {
  float time          = 0.0f;
  float waveHeight    = 0.3f;
  float waveSpeed     = 1.5f;
  float waveFreq      = 0.8f;

  float waveHeight2   = 0.15f;
  float waveSpeed2    = 1.0f;
  float waveFreq2     = 1.2f;
  float waveSteepness = 0.4f;

  Vector4 shallowColor = {0.1f, 0.5f, 0.6f, 0.85f};
  Vector4 deepColor    = {0.02f, 0.1f, 0.25f, 0.95f};

  float fresnelPower      = 3.0f;
  float specularPower     = 128.0f;
  float normalScrollSpeed = 0.03f;
  float normalStrength    = 0.6f;
};

// シングルトン的に定数バッファリソースを管理
static Microsoft::WRL::ComPtr<ID3D12Resource> s_waterCB;
static WaterParamsCB* s_waterCBMapped = nullptr;
static bool s_waterCBInitialized = false;

static void EnsureWaterCB() {
  if (s_waterCBInitialized) return;

  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;

  s_waterCB = CreateBufferResource(ctx.Device(), sizeof(WaterParamsCB),
                                   L"RC::WaterParamsCB");
  s_waterCB->Map(0, nullptr, reinterpret_cast<void**>(&s_waterCBMapped));
  if (s_waterCBMapped) {
    *s_waterCBMapped = WaterParamsCB{};
  }
  s_waterCBInitialized = true;
}

// ============================================================================
// API
// ============================================================================

int GenerateWaterPlane(float width, float height, uint32_t segments, int normalMapHandle) {
  auto &ctx = GetRenderContext();
  ModelData data = MeshGenerator::GeneratePlane(width, height, segments, segments);
  return ctx.PrimitiveMeshes().Create(data, normalMapHandle, "WaterPlane");
}

void DrawWater(int meshHandle, int normalMapHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;

  EnsureWaterCB();

  auto *m = ctx.PrimitiveMeshes().Get(meshHandle);
  if (!m) return;

  Matrix4x4 world = MakeAffineMatrix(m->T().scale, m->T().rotation, m->T().translation);
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  D3D12_GPU_VIRTUAL_ADDRESS waterCBAddr = s_waterCB ? s_waterCB->GetGPUVirtualAddress() : 0;
  BlendMode blend = ctx.CurrentBlendMode();

  const uint64_t key = SortKey::Make(SortKey::kLayerTranslucent, SortKey::HashPSO("water"), 0);
  ctx.PushCommand3D(key, [m, meshHandle, world, normalMapHandle, lightAddr, waterCBAddr, blend](ID3D12GraphicsCommandList *cl) {
    auto &ctx = GetRenderContext();
    auto prevBlend = ctx.CurrentBlendMode();
    ctx.SetBlendMode(blend);

    // 水面用パイプラインをバインド
    if (ctx.BindPipeline("water")) {
      ctx.BindCameraCB();
      cl->SetGraphicsRootConstantBufferView(3, lightAddr);
      ctx.BindAllLightCBs();

      // b6: WaterParams をバインド
      // Object3D と同じパラメータ (0〜10) を使用し、11番目に WaterParams を配置する
      if (waterCBAddr) {
        cl->SetGraphicsRootConstantBufferView(11, waterCBAddr);
      }

      // t4: InteractiveWave HeightMap をバインド (RootParameter 12)
      D3D12_GPU_DESCRIPTOR_HANDLE interactiveSrv = GetInteractiveWaterHeightMap();
      if (interactiveSrv.ptr != 0) {
        cl->SetGraphicsRootDescriptorTable(12, interactiveSrv);
      }

      ctx.PrimitiveMeshes().ApplyTexture(meshHandle, normalMapHandle);
      m->Draw(cl, world);
    }
    ctx.SetBlendMode(prevBlend);
  });
}

void UnloadWater(int meshHandle) {
  GetRenderContext().PrimitiveMeshes().Unload(meshHandle);
}

Transform *GetWaterTransformPtr(int meshHandle) {
  return GetRenderContext().PrimitiveMeshes().GetTransformPtr(meshHandle);
}

void SetWaterParams(float waveHeight, float waveSpeed, float waveFreq,
                    float waveHeight2, float waveSpeed2, float waveFreq2,
                    float waveSteepness,
                    const Vector4 &shallowColor, const Vector4 &deepColor,
                    float fresnelPower, float specularPower,
                    float normalScrollSpeed, float normalStrength) {
  EnsureWaterCB();
  if (!s_waterCBMapped) return;

  // time は別途 SetWaterTime で設定するので保持
  float savedTime = s_waterCBMapped->time;
  s_waterCBMapped->time = savedTime;
  s_waterCBMapped->waveHeight = waveHeight;
  s_waterCBMapped->waveSpeed = waveSpeed;
  s_waterCBMapped->waveFreq = waveFreq;
  s_waterCBMapped->waveHeight2 = waveHeight2;
  s_waterCBMapped->waveSpeed2 = waveSpeed2;
  s_waterCBMapped->waveFreq2 = waveFreq2;
  s_waterCBMapped->waveSteepness = waveSteepness;
  s_waterCBMapped->shallowColor = shallowColor;
  s_waterCBMapped->deepColor = deepColor;
  s_waterCBMapped->fresnelPower = fresnelPower;
  s_waterCBMapped->specularPower = specularPower;
  s_waterCBMapped->normalScrollSpeed = normalScrollSpeed;
  s_waterCBMapped->normalStrength = normalStrength;
}

void SetWaterEnvironmentCoefficient(int meshHandle, float coeff) {
  auto *m = GetRenderContext().PrimitiveMeshes().Get(meshHandle);
  if (!m) return;
  if (auto* mat = m->Mat()) {
    mat->environmentCoefficient = coeff;
  }
}

void SetWaterTime(float timeSec) {
  EnsureWaterCB();
  if (s_waterCBMapped) {
    s_waterCBMapped->time = timeSec;
  }
}

void TermWaterResources() {
  if (s_waterCB) {
    if (s_waterCBMapped) {
      s_waterCB->Unmap(0, nullptr);
      s_waterCBMapped = nullptr;
    }
    s_waterCB.Reset();
  }
  s_waterCBInitialized = false;
}

} // namespace RC
