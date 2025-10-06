#pragma once
#include <cassert>
#include <d3d12.h>

class DescriptorHeap; // 前方宣言（ヘッダ循環防止）

class DepthStencil {
public:
  // 深度/ステンシル作成 + DSVを外部ヒープに配置
  void Init(ID3D12Device *device, UINT width, UINT height,
            DescriptorHeap &dsvHeap,
            DXGI_FORMAT texFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
            DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);

  // リサイズ（GPUアイドル後に呼ぶこと）
  void Resize(UINT width, UINT height, DescriptorHeap &dsvHeap);

  // 破棄
  void Term();

  // クリア
  void Clear(ID3D12GraphicsCommandList *list, float depth = 1.0f,
             UINT8 stencil = 0) const;

  // アクセサ
  D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const { return dsv_; }
  ID3D12Resource *Resource() const { return tex_; }
  DXGI_FORMAT Format() const { return texFormat_; }
  UINT Width() const { return width_; }
  UINT Height() const { return height_; }

private:
  void createResource();
  void createView(DescriptorHeap &dsvHeap);

private:
  ID3D12Device *device_ = nullptr;    // 非所有
  ID3D12Resource *tex_ = nullptr;     // 所有
  D3D12_CPU_DESCRIPTOR_HANDLE dsv_{}; // 外部ヒープ内のスロット
  UINT width_ = 0, height_ = 0;
  DXGI_FORMAT texFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
  DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
};
