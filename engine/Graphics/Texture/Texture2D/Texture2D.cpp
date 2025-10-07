#include "Texture2D.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "DescriptorHeap/DescriptorHelpers.h"
#include "function/function.h"

void Texture2D::LoadFromFile(ID3D12Device *device, DescriptorHeap &srvHeap,
                             const std::string &path, bool srgb) {
  Term();

  // ---- 1) 画像読み込み（存在しなければ白1x1）----
  std::wstring wpath(path.begin(), path.end());
  DirectX::ScratchImage image;
  HRESULT hr = DirectX::LoadFromWICFile(
      wpath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
  if (FAILED(hr)) {
    // 白1x1を生成
    DirectX::ScratchImage white;
    white.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
    auto img = white.GetImage(0, 0, 0);
    uint8_t *p = img->pixels;
    p[0] = 255;
    p[1] = 255;
    p[2] = 255;
    p[3] = 255;
    mipImages_ = std::move(white);
  } else {
    // ミップ生成（失敗したら元画像を使用）
    DirectX::ScratchImage mips;
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                                  image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
                                  0, mips);
    mipImages_ = SUCCEEDED(hr) ? std::move(mips) : std::move(image);
  }

  metadata_ = mipImages_.GetMetadata();
  if (srgb)
    metadata_.format = DirectX::MakeSRGB(metadata_.format);

  // ---- 2) GPUテクスチャ作成（Upload書き戻しor DefaultでもOK）----
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION(metadata_.dimension);
  desc.Width = (UINT)metadata_.width;
  desc.Height = (UINT)metadata_.height;
  desc.DepthOrArraySize = (UINT16)metadata_.arraySize;
  desc.MipLevels = (UINT16)metadata_.mipLevels;
  desc.Format = metadata_.format;
  desc.SampleDesc.Count = 1;

  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_CUSTOM;
  heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
  heap.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

  hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                       nullptr, IID_PPV_ARGS(&resource_));
  assert(SUCCEEDED(hr));

  // ---- 3) サブリソースに書き込み ----
  for (size_t mip = 0; mip < metadata_.mipLevels; ++mip) {
    const DirectX::Image *img = mipImages_.GetImage(mip, 0, 0);
    hr = resource_->WriteToSubresource((UINT)mip, nullptr, img->pixels,
                                       (UINT)img->rowPitch,
                                       (UINT)img->slicePitch);
    assert(SUCCEEDED(hr));
  }

  // ---- 4) SRV作成 ----
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
