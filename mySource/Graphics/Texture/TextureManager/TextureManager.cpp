#include "TextureManager.h"
#include "DescriptorHeap/DescriptorHeap.h"

void TextureManager::Term() {
  for (auto &[_, tex] : cache_)
    tex.Term();
  cache_.clear();
  device_ = nullptr;
  srvHeap_ = nullptr;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::Load(const std::string &path,
                                                 bool srgb) {
  auto it = cache_.find(path);
  if (it != cache_.end() && it->second.IsLoaded()) {
    return it->second.GpuSrv();
  }
  // 新規 or 未ロード
  Texture2D &tex = cache_[path]; // 生成（未ロード）
  tex.LoadFromFile(device_, *srvHeap_, path, srgb);
  return tex.GpuSrv();
}

Texture2D *TextureManager::Get(const std::string &path) {
  auto it = cache_.find(path);
  if (it == cache_.end())
    return nullptr;
  return &it->second;
}
