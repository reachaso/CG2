#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include "../DescriptorHeap/DescriptorHeap.h"

class Dx12Core;

/// @brief シャドウマップ（深度テクスチャ）を生成・管理するクラス
class ShadowMap {
public:
  ShadowMap();
  ~ShadowMap();

  /// @brief シャドウマップを初期化する
  /// @param core Dx12Coreのインスタンス
  /// @param width テクスチャ幅
  /// @param height テクスチャ高さ
  void Create(Dx12Core* core, uint32_t width, uint32_t height);

  /// @brief コマンドリストにDSVをセットし、クリアする
  /// @param cmd コマンドリスト
  void BindAndClear(ID3D12GraphicsCommandList* cmd);

  /// @brief シャドウマップ描画後に、リソースをSRV状態へ遷移させる
  /// @param cmd コマンドリスト
  void TransitionToSRV(ID3D12GraphicsCommandList* cmd);

  /// @brief テクスチャリソースの取得
  ID3D12Resource* GetResource() const { return resource_.Get(); }

  /// @brief SRVヒープ上でのインデックスを取得
  uint32_t GetSrvIndex() const { return srvIndex_; }

  uint32_t GetWidth() const { return width_; }
  uint32_t GetHeight() const { return height_; }

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  DescriptorHeap dsvHeap_;
  uint32_t srvIndex_ = 0;
  Dx12Core* core_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};
