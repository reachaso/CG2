#include "Texture2D.h"
#include "Dx12/DescriptorHeap/DescriptorHeap.h"
#include "Dx12/DescriptorHeap/DescriptorHelpers.h"
#include "function/function.h"

void Texture2D::LoadFromFile(ID3D12Device *device, DescriptorHeap &srvHeap,
                             const std::string &path, bool srgb) {
  Term(); // 二重読み込みに備えてクリーン

  // 1) 読み込み（既存のユーティリティを使用）
  mipImages_ = LoadTexture(path.c_str()); // ScratchImage（move可能）
  metadata_ = mipImages_.GetMetadata();

  // 2) sRGB に寄せる（色テクスチャ想定）
  if (srgb) {
    metadata_.format = DirectX::MakeSRGB(metadata_.format);
  }

  // 3) GPU リソース作成 & 転送（既存のユーティリティを使用）
  resource_ = CreateTextureResource(device, metadata_);
  UploadTextureData(resource_, mipImages_);

  // 4) SRV 作成（Helpersで1行）
  createSRV(device, srvHeap);

  path_ = path;
}

void Texture2D::createSRV(ID3D12Device *device, DescriptorHeap &srvHeap) {
  cpuSrv_ = CreateSRV2D(device, srvHeap, resource_, metadata_.format,
                        static_cast<UINT>(metadata_.mipLevels));
  // 直近の割り当てインデックスの GPU ハンドルを取得
  gpuSrv_ = srvHeap.GPUAt(srvHeap.Used() - 1);
}

void Texture2D::Term() {
  if (resource_) {
    resource_->Release();
    resource_ = nullptr;
  }
  path_.clear();
  cpuSrv_ = {};
  gpuSrv_ = {};
  mipImages_ = DirectX::ScratchImage{};
  metadata_ = DirectX::TexMetadata{};
}
