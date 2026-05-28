#include "Game.h"
#include "imgui/imgui.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/Utility/ScreenCapture.h"
#include "Render/RenderContext.h"
#include <shellapi.h>
#include <filesystem>

// === 各シーン ===
#include "GameOverScene/GameOverScene.h"
#include "GameScene/GameScene.h"
#include "LightScene/LightScene.h"
#include "ResultScene/ResultScene.h"
#include "SampleScene/SampleScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"

void Game::Init(SceneContext &ctx) {
  sceneMgr_.Init(ctx);
  registerScenes_();

  // ここで最初のシーンを決める（Gameの責務）
#if defined(RC_DEVELOPMENT)
  const char *boot = "Select";
#elif defined(_DEBUG)
  const char *boot = "Sample";
#else
  const char *boot = "Title";
#endif
  sceneMgr_.ChangeImmediately(boot, ctx);
}

void Game::registerScenes_() {
  sceneMgr_.Register(std::make_unique<TitleScene>());
  sceneMgr_.Register(std::make_unique<SelectScene>());
  sceneMgr_.Register(std::make_unique<GameScene>());
  sceneMgr_.Register(std::make_unique<ResultScene>());
  sceneMgr_.Register(std::make_unique<GameOverScene>());
  sceneMgr_.Register(std::make_unique<SampleScene>());
  sceneMgr_.Register(std::make_unique<LightScene>());
}

void Game::registerAudioPaths_() {
  // 各音声の登録をここで行う
  // --- BGM ---

  // --- SE ---
}

void Game::Update(SceneContext &ctx) { sceneMgr_.Update(ctx); }

void Game::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  sceneMgr_.Render(ctx, cl);
}

void Game::Term() {
  sceneMgr_.Term();
  se_.Term();
  bgm_.Stop();
}

void Game::RequestChange(const std::string &name) {
  sceneMgr_.RequestChange(name);
}

const std::string &Game::CurrentSceneName() const {
  return sceneMgr_.CurrentName();
}

Scene* Game::GetCurrentScene() {
  return sceneMgr_.GetCurrentScene();
}

void Game::ReloadCurrentScene(SceneContext &ctx) {
  sceneMgr_.ReloadCurrentScene(ctx);
}

void Game::DrawDebugUI(SceneContext &ctx) {
#if RC_ENABLE_IMGUI
    if (ImGui::BeginMenu("Scene")) {
      const char *sceneNames[] = {"Title",    "Select", "Game",     "Result",
                                  "GameOver", "Sample", "Light"};
      const char *currentSceneName = CurrentSceneName().c_str();

      for (int i = 0; i < IM_ARRAYSIZE(sceneNames); i++) {
        bool is_selected = (strcmp(currentSceneName, sceneNames[i]) == 0);
        if (ImGui::MenuItem(sceneNames[i], nullptr, is_selected)) {
          RequestChange(sceneNames[i]);
        }
      }
      ImGui::EndMenu();
    }
#endif
}
