#include "SceneManager.h"
#include "Scene.h"
#include "Common/Log/Log.h"
#include "RenderCommon.h"
#include "Fade/Fade.h"
#include <chrono>
#include <format>
#include <cstdlib>

class FadeOutState;
class FadeInState;
class NormalState;
class LoadingState;

// For PostEffectType
#include "Graphics/PostProcess/PostProcess.h"

// =================================================================
// 状態インタフェース
// =================================================================
class ISceneState {
public:
  virtual ~ISceneState() = default;
  virtual void Update(Scene::SceneManager &sm, SceneContext &ctx) = 0;
  virtual void Render(Scene::SceneManager &sm, SceneContext &ctx,
                      ID3D12GraphicsCommandList *cl) = 0;
};

// =================================================================
// 各状態クラスの定義
// =================================================================

class NormalState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
};

class FadeOutState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
private:
  float counter_ = 0.0f;
};

class LoadingState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
private:
  bool loaded_ = false;
};

class FadeInState : public ISceneState {
public:
  void Update(Scene::SceneManager &sm, SceneContext &ctx) override;
  void Render(Scene::SceneManager &sm, SceneContext &ctx,
              ID3D12GraphicsCommandList *cl) override;
private:
  float counter_ = 0.0f;
};

// =================================================================
// NormalState 実装
// =================================================================
void NormalState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  // 常にシーンの Update は呼ぶ（カメラ更新等を止めない）
  if (sm.current_) {
    sm.current_->Update(sm, ctx);
  }

  if (!sm.requested_.empty()) {
    Log::Print("[SceneState] NormalState -> FadeOutState (requested: " + sm.requested_ + ")");
    
    // Dissolveエフェクトを開始
    RC::AddPostEffect(PostEffectType::Dissolve);
    
    // 固定ノイズ (インデックス0) を使用する
    RC::SetDissolveNoiseIndex(0);

    RC::SetDissolveThreshold(0.0f);
    RC::SetDissolveBaseColor(0.0f, 0.0f, 0.0f, 1.0f); // トランジションは黒で抜く
    
    sm.ChangeState(std::make_unique<FadeOutState>());
  }
}
void NormalState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                         ID3D12GraphicsCommandList *cl) {
  if (sm.current_) {
    sm.current_->Render(ctx, cl);
  }
}

// =================================================================
// FadeOutState 実装
// =================================================================
void FadeOutState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  counter_ += 1.0f / 60.0f;
  if (counter_ > sm.kFadeTime) counter_ = sm.kFadeTime;

  float threshold = counter_ / sm.kFadeTime;
  RC::SetDissolveThreshold(threshold);

  // フェードアウト中は旧シーンの Update も継続
  if (sm.current_ && counter_ < sm.kFadeTime) {
    sm.current_->Update(sm, ctx);
  }

  if (counter_ >= sm.kFadeTime) {
    Log::Print("[SceneState] FadeOutState -> LoadingState");
    sm.ChangeState(std::make_unique<LoadingState>());
  }
}
void FadeOutState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                          ID3D12GraphicsCommandList *cl) {
  // フェードアウト中は旧シーンを描画し続ける
  if (sm.current_) {
    sm.current_->Render(ctx, cl);
  } else {
    Log::Print("[SceneState] FadeOutState::Render - current_ is null!");
  }
}

// =================================================================
// LoadingState 実装
// - 画面が完全に黒い状態でシーン切り替えを行う
// - 切り替え完了後に FadeIn を開始する
// =================================================================
void LoadingState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  if (!loaded_) {
    // 画面が真っ黒（完全にDissolveされた状態）でシーン切り替え
    sm.ChangeImmediately(sm.requested_, ctx);
    loaded_ = true;

    // ChangeImmediately内でClearPostEffectsが呼ばれるため、再度Dissolveを適用
    RC::AddPostEffect(PostEffectType::Dissolve);
    RC::SetDissolveThreshold(1.0f);
    RC::SetDissolveBaseColor(0.0f, 0.0f, 0.0f, 1.0f); // トランジションは黒で抜く

    // フェードインを開始（Dissolveから復帰）
    sm.ChangeState(std::make_unique<FadeInState>());

    // 新シーンの初回 Update
    if (sm.current_) {
      sm.current_->Update(sm, ctx);
    }
  }
}
void LoadingState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                          ID3D12GraphicsCommandList *cl) {
  // 画面は完全にDissolveされた状態
  RC::PreDraw3D(ctx, cl);
  RC::PreDraw2D(ctx, cl);
}

// =================================================================
// FadeInState 実装
// =================================================================
void FadeInState::Update(Scene::SceneManager &sm, SceneContext &ctx) {
  counter_ += 1.0f / 60.0f;
  if (counter_ > sm.kFadeTime) counter_ = sm.kFadeTime;

  float threshold = 1.0f - (counter_ / sm.kFadeTime);
  RC::SetDissolveThreshold(threshold);

  // 新シーンの Update
  if (sm.current_) {
    sm.current_->Update(sm, ctx);
  }

  if (counter_ >= sm.kFadeTime) {
    RC::RemovePostEffect(PostEffectType::Dissolve);
    sm.ChangeState(std::make_unique<NormalState>());
  }
}
void FadeInState::Render(Scene::SceneManager &sm, SceneContext &ctx,
                         ID3D12GraphicsCommandList *cl) {
  // 新シーンを描画
  if (sm.current_) {
    sm.current_->Render(ctx, cl);
  } else {
    Log::Print("[SceneState] FadeInState::Render - current_ is null!");
  }
}

// =================================================================
// SceneManager 実装
// =================================================================

Scene::SceneManager::SceneManager() = default;
Scene::SceneManager::~SceneManager() = default;

void Scene::SceneManager::Init(SceneContext &ctx) {
  float width = float(ctx.app->width);
  float height = float(ctx.app->height);

  // Fadeコンポーネントを初期化
  fade_ = std::make_unique<Fade>();
  fade_->Init(ctx, width, height);

  // 初期状態をセット
  ChangeState(std::make_unique<NormalState>());
}

void Scene::SceneManager::Term() {
  if (fade_) {
    fade_.reset();
  }
  scenes_.clear();
  current_ = nullptr;
  currentName_.clear();
  requested_.clear();
  state_.reset();
}

void Scene::SceneManager::Register(std::unique_ptr<Scene> scene) {
  const std::string key = scene->Name();
  scenes_[key] = std::move(scene);
}

void Scene::SceneManager::RequestChange(const std::string &name) {
  requested_ = name;
}

void Scene::SceneManager::ChangeImmediately(const std::string &name,
                                            SceneContext &ctx) {
  Log::Print("[Scene] シーン切り替え: " + (currentName_.empty() ? "None" : currentName_) + " -> " + name);
  
  RC::ClearPostEffects();
  
  auto start = std::chrono::high_resolution_clock::now();

  if (current_) {
    current_->OnExit(ctx);
  }
  current_ = get_(name);
  if (current_) {
    current_->OnEnter(ctx);
    // ロード完了を自動待機（ユーザーが WaitAllLoads を書かなくても済むように）
    RC::WaitAllLoads();
  }
  currentName_ = name;
  requested_.clear();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> duration = end - start;
  Log::Print(std::format("[Scene] シーン切り替え完了 (Time: {:.3f}s)", duration.count()));

  // 新しいシーンでのテクスチャログ出力を許可するためにリセット
  RC::ClearTextureLogHistory();
}

void Scene::SceneManager::ReloadCurrentScene(SceneContext &ctx) {
  if (!current_) return;
  
  Log::Print("[Scene] シーンリロード: " + currentName_);
  
  RC::ClearPostEffects();
  
  current_->OnExit(ctx);
  current_->OnEnter(ctx);
  
  RC::WaitAllLoads();
  RC::ClearTextureLogHistory();
}

void Scene::SceneManager::Update(SceneContext &ctx) {
  // 常にFadeの更新は行う
  if (fade_) {
    fade_->Update();
  }

  // 現在の状態に更新処理を委譲
  if (state_) {
    state_->Update(*this, ctx);
  }
}

void Scene::SceneManager::Render(SceneContext &ctx,
                                 ID3D12GraphicsCommandList *cl) {
  // 現在の状態に描画処理を委譲
  if (state_) {
    state_->Render(*this, ctx, cl);
  }
}

void Scene::SceneManager::ChangeState(std::unique_ptr<ISceneState> newState) {
  state_ = std::move(newState);
}

Scene *Scene::SceneManager::get_(const std::string &name) {
  auto it = scenes_.find(name);
  return (it == scenes_.end()) ? nullptr : it->second.get();
}
