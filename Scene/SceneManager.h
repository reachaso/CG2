#pragma once
#include "Scene.h"
#include <memory>
#include <string>
#include <unordered_map>

class Scene::SceneManager {
public:
  void Register(std::unique_ptr<Scene> scene) {
    const std::string key = scene->Name();
    scenes_.emplace(key, std::move(scene));
  }

  // 次のフレームで切り替えたいシーン名を予約
  void RequestChange(const std::string &name) { requested_ = name; }

  // 即時切替（初期化でのみ使用）
  void ChangeImmediately(const std::string &name, SceneContext &ctx) {
    if (current_)
      current_->OnExit(ctx);
    current_ = get_(name);
    if (current_)
      current_->OnEnter(ctx);
    currentName_ = name;
    requested_.clear();
  }

  void Update(SceneContext &ctx) {
    // 遷移要求があればここで切替
    if (!requested_.empty()) {
      ChangeImmediately(requested_, ctx);
    }
    if (current_)
      current_->Update(*this, ctx);
  }

  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
    if (current_)
      current_->Render(ctx, cl);
  }

  const std::string &CurrentName() const { return currentName_; }

private:
  Scene *get_(const std::string &name) {
    auto it = scenes_.find(name);
    return (it == scenes_.end()) ? nullptr : it->second.get();
  }

  std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
  Scene *current_ = nullptr;
  std::string currentName_;
  std::string requested_;
};
