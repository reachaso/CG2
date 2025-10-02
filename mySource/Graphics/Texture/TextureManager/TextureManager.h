#pragma once
#include "../Texture2D/Texture2D.h"
#include <d3d12.h>
#include <string>
#include <unordered_map>

class DescriptorHeap;

class TextureManager {
public:
  void Init(ID3D12Device *device, DescriptorHeap *srvHeap) {
    device_ = device;
    srvHeap_ = srvHeap;
  }
  void Term();

  // 同じパスはキャッシュして再利用
  // 戻り値：GPU の SRV ハンドル（そのまま SetGraphicsRootDescriptorTable
  // に渡せる）
  D3D12_GPU_DESCRIPTOR_HANDLE Load(const std::string &path, bool srgb = true);

  // 直接 Texture2D にアクセスしたい場合
  Texture2D *Get(const std::string &path);

private:
  ID3D12Device *device_ = nullptr;    // 非所有
  DescriptorHeap *srvHeap_ = nullptr; // 非所有（可視ヒープ）
  std::unordered_map<std::string, Texture2D> cache_;
};
