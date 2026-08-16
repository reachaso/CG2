#include "SpriteManager.h"

#include "Math/Math.h"
#include "Texture/TextureManager/TextureManager.h"
#include "Common/Log/Log.h"

namespace RC {

void SpriteManager::Init(ID3D12Device *device, TextureManager *texman) {
  device_ = device;
  texman_ = texman;
  sprites_.clear();
  quad_.reset();
}

void SpriteManager::Term() {
  sprites_.clear();
  quad_.reset();
  device_ = nullptr;
  texman_ = nullptr;
}

std::shared_ptr<SpriteMesh2D> SpriteManager::EnsureQuad_() {
  if (!device_) {
    return nullptr;
  }

  if (!quad_) {
    quad_ = std::make_shared<SpriteMesh2D>();
    quad_->Initialize(device_);
  }
  return quad_;
}

bool SpriteManager::IsValid(int handle) const {
  return (handle >= 0 && handle < static_cast<int>(sprites_.size()) &&
          sprites_[handle].inUse && sprites_[handle].ptr);
}

Sprite2D *SpriteManager::Get(int handle) {
  if (!IsValid(handle)) {
    return nullptr;
  }
  auto& s = sprites_[handle];
  // 非同期ロード完了後にSRVハンドルを遅延取得する
  if (s.ptr->GetTexture().ptr == 0 && s.texHandle >= 0 && texman_) {
    s.ptr->SetTexture(texman_->GetSrv(s.texHandle));
  }
  return s.ptr.get();
}

const Sprite2D *SpriteManager::Get(int handle) const {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return sprites_[handle].ptr.get();
}

int SpriteManager::Load(const std::string &path, float screenW, float screenH,
                        bool srgb) {
  if (!device_ || !texman_) {
    return -1;
  }

  std::string npath = Log::NormalizePath(path);
  // テクスチャロード（TextureManager 側がキャッシュしている想定）
  const int texHandle = texman_->LoadID(npath, srgb);
  if (texHandle < 0) {
    return -1;
  }

  // 現状は RenderCommon と同じく「スロット再利用なし」
  const int handle = static_cast<int>(sprites_.size());
  sprites_.emplace_back();

  auto &s = sprites_[handle];
  s.ptr = std::make_unique<Sprite2D>();

  const auto quad = EnsureQuad_();
  if (!quad) {
    // 失敗時はスロットを無効化して返す
    s.ptr.reset();
    s.inUse = false;
    s.texHandle = -1;
    return -1;
  }

  // 初期化
  s.ptr->Initialize(device_, quad, screenW, screenH);
  s.ptr->SetTexture(texman_->GetSrv(texHandle));
  s.ptr->SetFilePath(npath);
  Log::Print("[Sprite] ロード完了: " + npath);

  // 初期値（必要なら外側で SetTransform/SetSize 等で上書き）
  s.ptr->SetSize(100, 100);
  s.ptr->T().translation = {0, 0, 0};
  s.ptr->SetVisible(true);

  s.texHandle = texHandle;
  s.inUse = true;
  return handle;
}

void SpriteManager::Unload(int handle) {
  if (handle < 0 || handle >= static_cast<int>(sprites_.size())) {
    return;
  }
  auto &s = sprites_[handle];
  if (!s.inUse) {
    return;
  }
  Log::Print("[Sprite] 破棄完了: " + Log::NormalizePath(s.ptr->GetFilePath()));
  s.ptr.reset();
  s.inUse = false;
  s.texHandle = -1;
}

void SpriteManager::Draw(int handle, ID3D12GraphicsCommandList *cl) {
  if (!cl) {
    return;
  }
  auto *sp = Get(handle);
  if (!sp) {
    return;
  }

  // DrawRect / DrawRectUV は UVTransform を残したまま抜けるので、
  // 全体描画のときは毎回 identity へ戻す（同じハンドルを使い回せるようにする）
  sp->UVTransform() = MakeIdentity4x4();

  sp->Update();
  sp->Draw(cl);
}

void SpriteManager::DrawRect(int handle, float srcX, float srcY, float srcW,
                            float srcH, float texW, float texH, float insetPx,
                            ID3D12GraphicsCommandList *cl) {
  if (!cl) {
    return;
  }

  auto *sp = Get(handle);
  if (!sp) {
    return;
  }

  // 非同期ロード完了後にSRVハンドルを遅延取得する
  if (sp->GetTexture().ptr == 0 && sprites_[handle].texHandle >= 0 && texman_) {
    sp->SetTexture(texman_->GetSrv(sprites_[handle].texHandle));
  }

  // 入力が壊れている場合は安全に無視
  if (texW <= 0.0f || texH <= 0.0f || srcW <= 0.0f || srcH <= 0.0f) {
    return;
  }

  // にじみ対策（必要なら 0.5px 程度を指定）
  if (insetPx != 0.0f) {
    srcX += insetPx;
    srcY += insetPx;
    srcW -= insetPx * 2.0f;
    srcH -= insetPx * 2.0f;
    if (srcW <= 0.0f || srcH <= 0.0f) {
      return;
    }
  }

  // ピクセル -> UV(0..1)
  const float u0 = srcX / texW;
  const float v0 = srcY / texH;
  const float su = srcW / texW;
  const float sv = srcH / texH;

  // UVTransform は定数バッファへの直接参照であり、Draw() はコマンドリストへ
  // 「記録」するだけで GPU がそれを読むのはフレーム終端の実行時。
  // ここで描画後に元の値へ戻すと、GPU が読む頃には戻したあとの値になっており、
  // 指定した切り出し矩形がまったく効かない。したがって差し替えたまま抜ける。
  // 全体描画へ戻したい場合は Draw() が identity に戻すので問題ない。
  // uv' = uv * Scale + Translate（row-vector 前提）
  Matrix4x4 uvM = MakeIdentity4x4();
  uvM = Multiply(MakeScaleMatrix(Vector3{su, sv, 1.0f}), uvM);
  uvM = Multiply(uvM, MakeTranslateMatrix(Vector3{u0, v0, 0.0f}));
  sp->UVTransform() = uvM;

  sp->Update();
  sp->Draw(cl);
}

void SpriteManager::DrawRectUV(int handle, float u0, float v0, float u1,
                              float v1, ID3D12GraphicsCommandList *cl) {
  if (!cl) {
    return;
  }

  auto *sp = Get(handle);
  if (!sp) {
    return;
  }

  const float su = (u1 - u0);
  const float sv = (v1 - v0);
  if (su <= 0.0f || sv <= 0.0f) {
    return;
  }

  // DrawRect と同じ理由で、描画後に UVTransform を戻さない
  Matrix4x4 uvM = MakeIdentity4x4();
  uvM = Multiply(MakeScaleMatrix(Vector3{su, sv, 1.0f}), uvM);
  uvM = Multiply(uvM, MakeTranslateMatrix(Vector3{u0, v0, 0.0f}));
  sp->UVTransform() = uvM;

  sp->Update();
  sp->Draw(cl);
}

void SpriteManager::SetTransform(int handle, const Transform &t) {
  auto *sp = Get(handle);
  if (!sp) {
    return;
  }
  sp->T() = t;
}

void SpriteManager::SetColor(int handle, const Vector4 &color) {
  auto *sp = Get(handle);
  if (!sp) {
    return;
  }
  sp->SetColor(color);
}

void SpriteManager::SetSize(int handle, float w, float h) {
  auto *sp = Get(handle);
  if (!sp) {
    return;
  }
  sp->SetSize(w, h);
}

void SpriteManager::DrawImGui(int handle, const char *name) {
  auto *sp = Get(handle);
  if (!sp) {
    return;
  }
  sp->DrawImGui(name);
}

int SpriteManager::GetTexHandle(int handle) const {
  if (!IsValid(handle)) {
    return -1;
  }
  return sprites_[handle].texHandle;
}

} // namespace RC
