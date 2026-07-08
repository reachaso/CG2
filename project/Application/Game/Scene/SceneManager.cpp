#include "SceneManager.h"
#include "Scene.h"
#include "Common/Log/Log.h"
#include "RenderCommon.h"
#include "Fade/Fade.h"
#include "Camera/CameraController.h"
#include "DataDrivenScene/DataDrivenScene.h"
#include <chrono>
#include <format>
#include <cstdlib>
#include "Render/RenderContext.h"

namespace RC { class CameraController; }

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

  // エディタカメラの初期化（全シーンで共有）
  camera_ = std::make_unique<RC::CameraController>();
  camera_->Initialize(ctx.input, RC::Vector3{0.0f, 0.35f, -15.0f},
                      RC::Vector3{0, 0, 0}, 0.45f, width / height, 0.1f, 100.0f);
  camera_->SetUseDebug(true);
  ctx.camera = camera_.get();

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
  std::chrono::duration<double> elapsed = end - start;
  Log::Print(std::format("[Scene] Load time: {:.4f} sec", elapsed.count()));

  requested_.clear();

  // 新しいシーンでのテクスチャログ出力を許可するためにリセット
  RC::ClearTextureLogHistory();

  // シーン切り替え直後のフレームで描画コマンドの順序をダンプする
  RC::GetRenderContext().RequestDumpCommandOrder();
}

void Scene::SceneManager::ReloadCurrentScene(SceneContext &ctx) {
  if (current_) {
    Log::Print("[Scene] Reloading scene: " + currentName_);
    RC::ClearPostEffects();
    current_->OnExit(ctx);
    current_->OnEnter(ctx);
    RC::WaitAllLoads();
    RC::ClearTextureLogHistory();
  }
}

void Scene::SceneManager::BackupCurrentScene() {
    if (current_) {
        // dynamic_cast checking if it's DataDrivenScene
        if (auto* dds = dynamic_cast<DataDrivenScene*>(current_)) {
            dds->BackupState();
            Log::Print("[Scene] Backup created for: " + currentName_);
        }
    }
}

void Scene::SceneManager::RestoreCurrentScene(SceneContext &ctx) {
    if (current_) {
        if (auto* dds = dynamic_cast<DataDrivenScene*>(current_)) {
            Log::Print("[Scene] Restoring scene from backup: " + currentName_);
            RC::ClearPostEffects();
            dds->RestoreState(ctx);
        }
    }
}

void Scene::SceneManager::Update(SceneContext &ctx) {
  // 常にFadeの更新は行う
  if (fade_) {
    fade_->Update();
  }

  // 現在の状態に更新処理を委譲（シーンのUpdate内でカメラモード切替等が行われる）
  if (state_) {
    state_->Update(*this, ctx);
  }

  // カメラ更新 & ビュー/プロジェクション反映（全シーン共通）
  if (camera_) {
    camera_->Update();
    RC::SetCamera(camera_->GetView(), camera_->GetProjection(), camera_->GetWorldPos());
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

// =================================================================
// Data-driven scene management
// =================================================================
#include "DataDrivenScene/DataDrivenScene.h"
#include <filesystem>

void Scene::SceneManager::LoadScenesFromDirectory(const std::string& dirPath) {
  namespace fs = std::filesystem;
  if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
    Log::Print("[SceneManager] Scene directory not found: " + dirPath);
    return;
  }

  for (auto& entry : fs::directory_iterator(dirPath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      std::string sceneName = entry.path().stem().string();
      if (scenes_.count(sceneName) > 0) continue;

      auto scene = std::make_unique<DataDrivenScene>(sceneName, entry.path().string());
      Log::Print("[SceneManager] Registered data-driven scene: " + sceneName);
      scenes_[sceneName] = std::move(scene);
    }
  }
}

bool Scene::SceneManager::CreateNewScene(const std::string& name, const std::string& dirPath) {
  if (scenes_.count(name) > 0) {
    Log::Print("[SceneManager] Scene already exists: " + name);
    return false;
  }

  namespace fs = std::filesystem;
  fs::create_directories(dirPath);
  std::string filePath = dirPath + "/" + name + ".json";

  auto scene = std::make_unique<DataDrivenScene>(name, filePath);

  std::string templatePath = dirPath + "/../Template/SceneTemplate.json";
  if (fs::exists(templatePath)) {
    fs::copy_file(templatePath, filePath, fs::copy_options::overwrite_existing);
    Log::Print("[SceneManager] Created new scene from template: " + templatePath);
    scene->Load();
  } else {
    // デフォルトのエンティティを追加 (Unityライクな初期状態)
    auto dirLight = scene->CreateEntity("Directional Light");
    auto& dl = dirLight->AddComponent<DirectionalLightComponent>();
    dl.color = {1.0f, 1.0f, 1.0f, 1.0f};
    dl.direction = {0.0f, -1.0f, 0.5f}; // 斜め下
    dl.intensity = 1.0f;

    auto mainCam = scene->CreateEntity("Main Camera");
    auto& cam = mainCam->AddComponent<CameraComponent>();
    cam.isMain = true;
    
    if (auto* tr = mainCam->GetComponent<TransformComponent>()) {
        tr->position = {0.0f, 1.0f, -10.0f};
    }

    scene->Save();
  }

  Log::Print("[SceneManager] Created new scene: " + name);
  scenes_[name] = std::move(scene);
  return true;
}

bool Scene::SceneManager::DeleteScene(const std::string& name) {
  auto it = scenes_.find(name);
  if (it == scenes_.end()) {
    Log::Print("[SceneManager] Scene not found for deletion: " + name);
    return false;
  }

  if (current_ == it->second.get()) {
    Log::Print("[SceneManager] Cannot delete active scene: " + name);
    return false;
  }

  if (auto* dds = dynamic_cast<DataDrivenScene*>(it->second.get())) {
    namespace fs = std::filesystem;
    if (fs::exists(dds->FilePath())) {
      fs::remove(dds->FilePath());
      Log::Print("[SceneManager] Deleted scene file: " + dds->FilePath());
    }
  }

  scenes_.erase(it);
  Log::Print("[SceneManager] Deleted scene: " + name);
  return true;
}

bool Scene::SceneManager::SaveCurrentScene() {
  if (!current_) return false;
  if (auto* dds = dynamic_cast<DataDrivenScene*>(current_)) {
    return dds->Save();
  }
  Log::Print("[SceneManager] Current scene is not a DataDrivenScene, cannot save.");
  return false;
}
