#pragma once
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>

class Device {
public:
  // enableDebug: D3D12 デバッグレイヤーを有効化
  // gpuValidation: GPU バリデーション（重い）を任意でON
  void Init(bool enableDebug = true, bool gpuValidation = false);
  void Term();

  // ハンドル取得（非所有）
  IDXGIFactory6 *Factory() const { return factory_; }
  IDXGIAdapter4 *Adapter() const { return adapter_; }
  ID3D12Device *GetDevice() const { return device_; }
  D3D_FEATURE_LEVEL FeatureLevel() const { return featureLevel_; }
  const char *FeatureLevelString() const;

  // 便利関数
  bool IsTearingSupported() const; // DXGI_FEATURE_PRESENT_ALLOW_TEARING
  void SetupInfoQueue(bool breakOnError = true,
                      bool muteInfo = true); // ログ抑制など

private:
  void enableDebug_(bool gpuValidation);
  IDXGIAdapter4 *pickHardwareAdapter_(); // HighPerformance 優先で列挙
  IDXGIAdapter4 *getWarpAdapter_();      // WARP 取得（最終手段）

private:
  IDXGIFactory6 *factory_ = nullptr; // 所有
  IDXGIAdapter4 *adapter_ = nullptr; // 所有
  ID3D12Device *device_ = nullptr;   // 所有
  D3D_FEATURE_LEVEL featureLevel_ = (D3D_FEATURE_LEVEL)0;
};
