#include "RenderInteractiveWater.h"
#include "RenderContext.h"
#include "RenderCommon.h"
#include "Dx12/PipelineManager.h"
#include "Dx12/SRVManager/SRVManager.h"
#include "Dx12/Dx12Core.h" // GetSRVManager 等が取れると仮定
#include "Scene.h"
#include "Common/Log/Log.h"
#include "Common/function/function.h"
#include <algorithm>

namespace RC {

struct WaveSimCB {
  float alpha;
  float damping;
  int sourceCount;
  float padding;
  Vector4 sources[64];
};

static constexpr int TEX_SIZE = 256;
static Microsoft::WRL::ComPtr<ID3D12Resource> s_heightMaps[3];
static SRVManager::Handle s_srvs[3];
static SRVManager::Handle s_uavs[3];
static int s_currIdx = 0; // 最新のハイトマップのインデックス

static Microsoft::WRL::ComPtr<ID3D12Resource> s_simCB;
static WaveSimCB* s_simCBMapped = nullptr;

static std::vector<WaveSource> s_pendingSources;
static bool s_initialized = false;
static int s_resetFrames = 3; // 最初の3フレームはテクスチャを0クリアする

// ヘルパー: D3D12リソース作成
static Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVTexture2D(ID3D12Device* device, int width, int height, DXGI_FORMAT format) {
  Microsoft::WRL::ComPtr<ID3D12Resource> res;
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Alignment = 0;
  desc.Width = width;
  desc.Height = height;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      nullptr,
      IID_PPV_ARGS(&res));
  if (FAILED(hr)) return nullptr;
  return res;
}

// CreateBufferResource is already defined in function.h

void InitInteractiveWater() {
  if (s_initialized) return;
  auto& ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  auto device = ctx.Device();

  // テクスチャリソースの作成
  for (int i = 0; i < 3; ++i) {
    s_heightMaps[i] = CreateUAVTexture2D(device, TEX_SIZE, TEX_SIZE, DXGI_FORMAT_R32_FLOAT);
    s_srvs[i] = ctx.Ctx()->core->SRVMan().CreateTexture2D(s_heightMaps[i].Get(), DXGI_FORMAT_R32_FLOAT, 1);
    s_uavs[i] = ctx.Ctx()->core->SRVMan().CreateTexture2DUAV(s_heightMaps[i].Get(), DXGI_FORMAT_R32_FLOAT);
  }

  // 定数バッファの作成
  s_simCB = CreateBufferResource(device, sizeof(WaveSimCB), L"WaveSimCB");
  s_simCB->Map(0, nullptr, reinterpret_cast<void**>(&s_simCBMapped));
  if (s_simCBMapped) {
    s_simCBMapped->alpha = 0.45f;
    s_simCBMapped->damping = 0.985f;
    s_simCBMapped->sourceCount = 0;
  }

  s_currIdx = 0;
  s_resetFrames = 3; // 初期化時にリセットカウンタを設定
  s_initialized = true;
  Log::Print("[RenderInteractiveWater] Initialized");
}

void TermInteractiveWater() {
  if (!s_initialized) return;

  auto& ctx = GetRenderContext();
  for (int i = 0; i < 3; ++i) {
    if (ctx.Ctx() && ctx.Ctx()->core) {
      ctx.Ctx()->core->SRVMan().Free(s_srvs[i]);
      ctx.Ctx()->core->SRVMan().Free(s_uavs[i]);
    }
    s_heightMaps[i].Reset();
  }

  if (s_simCB) {
    s_simCB->Unmap(0, nullptr);
    s_simCBMapped = nullptr;
    s_simCB.Reset();
  }

  s_initialized = false;
  Log::Print("[RenderInteractiveWater] Terminated");
}

void AddWaveSource(const WaveSource& source) {
  if (s_pendingSources.size() < 64) {
    s_pendingSources.push_back(source);
  }
}

void UpdateInteractiveWater() {
  if (!s_initialized) return;
  auto& ctx = GetRenderContext();

  // インデックスのローテーション
  // s_currIdx は前フレームでレンダリングした「最新(h1)」のテクスチャ
  int h1Idx = s_currIdx;
  // 2フレーム前の「古い(h2)」テクスチャ
  int h2Idx = (s_currIdx + 2) % 3;
  // 今回レンダリングする「次(out)」のテクスチャ
  int nextIdx = (s_currIdx + 1) % 3;

  // CBの更新
  if (s_simCBMapped) {
    s_simCBMapped->alpha = 0.15f;   // 波の伝播速度（穏やかな広がり）
    s_simCBMapped->damping = 0.95f; // 速度減衰（0.95^60≈0.05、約1秒でほぼ消滅）
    
    // リセット処理
    if (s_resetFrames > 0) {
        s_simCBMapped->padding = 1.0f;
        s_resetFrames--;
    } else {
        s_simCBMapped->padding = 0.0f;
    }

    int count = std::min(64, (int)s_pendingSources.size());
    s_simCBMapped->sourceCount = count;
    for (int i = 0; i < count; ++i) {
      s_simCBMapped->sources[i] = Vector4(
          s_pendingSources[i].uv.x,
          s_pendingSources[i].uv.y,
          s_pendingSources[i].radius,
          s_pendingSources[i].strength);
    }
  }
  s_pendingSources.clear();

  // リソースバリアから先のGPUコマンドをキューイング (SortKey=0で最初に処理させる)
  ctx.PushCommand3D(0, [h1Idx, h2Idx, nextIdx](ID3D12GraphicsCommandList* cl) {
    auto& renderCtx = GetRenderContext();
    if (!cl) return;

    // リソースバリアの設定 (nextIdx を UAV に)
    D3D12_RESOURCE_BARRIER barrierToUAV = {};
    barrierToUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToUAV.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToUAV.Transition.pResource = s_heightMaps[nextIdx].Get();
    barrierToUAV.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrierToUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToUAV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &barrierToUAV);

    // Compute Shaderのディスパッチ
    ID3D12PipelineState* pso = nullptr;
    ID3D12RootSignature* root = nullptr;
    if (renderCtx.Ctx() && renderCtx.Ctx()->pipelineManager) {
      pso = renderCtx.Ctx()->pipelineManager->GetComputePSO("wave_simulation");
      root = renderCtx.Ctx()->pipelineManager->GetComputeRoot("wave_simulation");
    }

    if (pso && root) {
      cl->SetPipelineState(pso);
      cl->SetComputeRootSignature(root);

      // b0
      cl->SetComputeRootConstantBufferView(0, s_simCB->GetGPUVirtualAddress());

      // t0 (h1: 最新のハイトマップ)
      cl->SetComputeRootDescriptorTable(1, s_srvs[h1Idx].gpu);
      // t1 (h2: 1つ前のハイトマップ)
      cl->SetComputeRootDescriptorTable(2, s_srvs[h2Idx].gpu);
      // u0 (next)
      cl->SetComputeRootDescriptorTable(3, s_uavs[nextIdx].gpu);

      // Dispatch (256x256のテクスチャで16x16のスレッドグループ)
      cl->Dispatch(TEX_SIZE / 16, TEX_SIZE / 16, 1);
    }

    // UAVバリアで完了待ち
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = s_heightMaps[nextIdx].Get();
    cl->ResourceBarrier(1, &uavBarrier);

    // リソースバリアの復元 (nextIdx を SRV に戻す)
    D3D12_RESOURCE_BARRIER barrierToSRV = {};
    barrierToSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToSRV.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToSRV.Transition.pResource = s_heightMaps[nextIdx].Get();
    barrierToSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrierToSRV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &barrierToSRV);
  }, "InteractiveWater_WaveSim");

  // 更新されたものを次回の s_currIdx とする
  s_currIdx = nextIdx;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetInteractiveWaterHeightMap() {
  if (!s_initialized) return {};
  return s_srvs[s_currIdx].gpu;
}

} // namespace RC
