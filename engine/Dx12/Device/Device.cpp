#include "Device.h"
#include "Log/Log.h"
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <format>

extern Log logger;
using Microsoft::WRL::ComPtr;

void Device::enableDebug_(bool gpuValidation) {
#ifdef _DEBUG
  ComPtr<ID3D12Debug1> debug;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
    debug->EnableDebugLayer();
    if (gpuValidation) {
      debug->SetEnableGPUBasedValidation(TRUE);
    }
  }
#endif
}

IDXGIAdapter4 *Device::pickHardwareAdapter_() {
  IDXGIAdapter4 *result = nullptr;

  ComPtr<IDXGIFactory6> f6;
  factory_->QueryInterface(IID_PPV_ARGS(&f6));

  for (UINT i = 0;; ++i) {
    ComPtr<IDXGIAdapter1> a1;
    if (FAILED(f6->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&a1)))) {
      break;
    }
    DXGI_ADAPTER_DESC1 d{};
    a1->GetDesc1(&d);
    if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      continue; // ソフトウェアは除外

    // とりあえず 12.1 を試し、ダメなら 12.0 でも OK
    ComPtr<ID3D12Device> probe;
    if (SUCCEEDED(D3D12CreateDevice(a1.Get(), D3D_FEATURE_LEVEL_12_1,
                                    IID_PPV_ARGS(&probe))) ||
        SUCCEEDED(D3D12CreateDevice(a1.Get(), D3D_FEATURE_LEVEL_12_0,
                                    IID_PPV_ARGS(&probe)))) {
      a1->QueryInterface(IID_PPV_ARGS(&result)); // IDXGIAdapter4 に昇格
      break;
    }
  }
  if (result)
    result->AddRef();
  return result;
}

IDXGIAdapter4 *Device::getWarpAdapter_() {
  IDXGIAdapter4 *out = nullptr;
  ComPtr<IDXGIAdapter> warp;
  if (SUCCEEDED(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp)))) {
    warp->QueryInterface(IID_PPV_ARGS(&out));
  }
  if (out)
    out->AddRef();
  return out;
}

void Device::Init(bool enableDebug, bool gpuValidation) {
  if (enableDebug)
    enableDebug_(gpuValidation);

  UINT factoryFlags = 0;
#ifdef _DEBUG
  if (enableDebug)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory_));
  assert(SUCCEEDED(hr));

  adapter_ = pickHardwareAdapter_();
  if (!adapter_) {
    adapter_ = getWarpAdapter_();
    assert(adapter_ && "No suitable DXGI adapter found");
  }

  // ===== ここでアダプタ名をログ =====
  DXGI_ADAPTER_DESC3 desc{};
  {
    // GetDesc3 は IDXGIAdapter4
    hr = adapter_->GetDesc3(&desc);
    assert(SUCCEEDED(hr));
    // logger.ConvertString(L"...") で wstring を UTF-8
    // に（あなたの実装に合わせる）
    logger.WriteLog(logger.ConvertString(
        std::format(L"Use Adapter: {}\n", desc.Description)));
  }

  // ===== FeatureLevel を高い順で試す & ログ =====
  const D3D_FEATURE_LEVEL tryLevels[] = {
      D3D_FEATURE_LEVEL_12_2,
      D3D_FEATURE_LEVEL_12_1,
      D3D_FEATURE_LEVEL_12_0,
  };
  const char *tryLevelStr[] = {"12.2", "12.1", "12.0"};

  ID3D12Device *dev = nullptr;
  for (size_t i = 0; i < _countof(tryLevels); ++i) {
    hr = D3D12CreateDevice(adapter_, tryLevels[i], IID_PPV_ARGS(&dev));
    if (SUCCEEDED(hr)) {
      featureLevel_ = tryLevels[i];
      device_ = dev; // 所有
      logger.WriteLog(std::format("FeatureLevel : {}\n", tryLevelStr[i]));
      break;
    }
  }
  assert(device_ && "D3D12 device creation failed");
  logger.WriteLog("Complete create D3D12Device!!!\n");
}


void Device::SetupInfoQueue(bool breakOnError, bool muteInfo) {
#ifdef _DEBUG
  if (!device_)
    return;
  ComPtr<ID3D12InfoQueue> iq;
  if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&iq)))) {
    if (muteInfo) {
      D3D12_MESSAGE_SEVERITY sev[] = {D3D12_MESSAGE_SEVERITY_INFO};
      D3D12_INFO_QUEUE_FILTER f{};
      f.DenyList.NumSeverities = _countof(sev);
      f.DenyList.pSeverityList = sev;
      iq->PushStorageFilter(&f);
    }
    if (breakOnError) {
      iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
      iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
      iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
    }
  }
#endif
}

// 表示用の文字列
const char *Device::FeatureLevelString() const {
  switch (featureLevel_) {
  case D3D_FEATURE_LEVEL_12_2:
    return "12.2";
  case D3D_FEATURE_LEVEL_12_1:
    return "12.1";
  case D3D_FEATURE_LEVEL_12_0:
    return "12.0";
  default:
    return "Unknown";
  }
}

bool Device::IsTearingSupported() const {
  BOOL supported = FALSE;
  if (factory_) {
    factory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                  &supported, sizeof(supported));
  }
  return supported == TRUE;
}

void Device::Term() {
  if (device_) {
    device_->Release();
    device_ = nullptr;
  }
  if (adapter_) {
    adapter_->Release();
    adapter_ = nullptr;
  }
  if (factory_) {
    factory_->Release();
    factory_ = nullptr;
  }
}
