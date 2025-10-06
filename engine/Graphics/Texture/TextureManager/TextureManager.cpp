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

// TextureManager.cpp
TextureManager::TextureID TextureManager::LoadID(const std::string &path,
                                                 bool srgb) {
  // 既にIDがあるならそれを返す
  auto itId = pathToId_.find(path);
  if (itId != pathToId_.end()) {
    // 読み込み済みでない可能性もあるので cache_ を確認し、未ロードならロード
    auto itTex = cache_.find(path);
    if (itTex == cache_.end() || !itTex->second.IsLoaded()) {
      Texture2D &tex = cache_[path];
      tex.LoadFromFile(device_, *srvHeap_, path, srgb);
    }
    return itId->second;
  }

  // まだIDが無ければ生成・ロード
  Texture2D &tex = cache_[path];
  if (!tex.IsLoaded()) {
    tex.LoadFromFile(device_, *srvHeap_, path, srgb); // SRV作成まで内部で実施
  }
  TextureID id = nextId_++;
  pathToId_[path] = id;
  idToPath_[id] = path;
  return id;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrv(TextureID id) const {
  static D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{}; // 失敗時用
  auto it = idToPath_.find(id);
  if (it == idToPath_.end())
    return nullHandle;
  auto itTex = cache_.find(it->second);
  if (itTex == cache_.end() || !itTex->second.IsLoaded())
    return nullHandle;
  return itTex->second.GpuSrv(); // 既存APIで取得
}

const DirectX::TexMetadata *TextureManager::GetMeta(TextureID id) const {
  auto it = idToPath_.find(id);
  if (it == idToPath_.end())
    return nullptr;
  auto itTex = cache_.find(it->second);
  if (itTex == cache_.end() || !itTex->second.IsLoaded())
    return nullptr;
  return &itTex->second.Metadata(); // 既存の取得関数を流用
}

Texture2D *TextureManager::GetTexture(TextureID id) {
  auto it = idToPath_.find(id);
  if (it == idToPath_.end())
    return nullptr;
  return Get(it->second);
}
