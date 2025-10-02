#pragma once
#include "../../../../externals/DirectXTex/DirectXTex.h"
#include <cassert>
#include <d3d12.h>
#include <string>

class DescriptorHeap; // 循環防止の前方宣言

class Texture2D {
public:
  Texture2D() = default;
  ~Texture2D() { Term(); }

  // ファイルから読み込んで GPU リソース + SRV を作成
  // srgb=true ならフォーマットを DirectX::MakeSRGB で寄せる
  void LoadFromFile(ID3D12Device *device, DescriptorHeap &srvHeap,
                    const std::string &path, bool srgb = true);

  void Term();

  // 取得系
  ID3D12Resource *Resource() const { return resource_; }
  D3D12_GPU_DESCRIPTOR_HANDLE GpuSrv() const { return gpuSrv_; }
  D3D12_CPU_DESCRIPTOR_HANDLE CpuSrv() const { return cpuSrv_; }
  const DirectX::TexMetadata &Metadata() const { return metadata_; }
  const std::string &Path() const { return path_; }
  bool IsLoaded() const { return resource_ != nullptr; }

private:
  // 内部ユーティリティ（実体は既存の関数群を利用）
  void createSRV(ID3D12Device *device, DescriptorHeap &srvHeap);

private:
  std::string path_;
  ID3D12Resource *resource_ = nullptr; // 所有
  DirectX::ScratchImage mipImages_;
  DirectX::TexMetadata metadata_{};

  D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv_{};
  D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv_{};
};
