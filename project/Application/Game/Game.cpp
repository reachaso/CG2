#include "Game.h"
#include "imgui/imgui.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/Utility/ScreenCapture.h"
#include "Render/RenderContext.h"
#include <shellapi.h>
#include <filesystem>

// C++ hardcoded scenes (scenes with rich runtime logic)
// (All scenes have been moved to DataDrivenScene via JSON)

void Game::Init(SceneContext &ctx) {
  sceneMgr_.Init(ctx);
  registerScenes_();

  // Here we decide the first scene to boot into (Game's responsibility)
#if defined(RC_DEVELOPMENT)
  const char *boot = "Select";
#elif defined(_DEBUG)
  const char *boot = "Game";
#else
  const char *boot = "Select";
#endif
  sceneMgr_.ChangeImmediately(boot, ctx);
}

void Game::registerScenes_() {
  // Load data-driven scenes from JSON directory (editor-managed)
  sceneMgr_.LoadScenesFromDirectory(kSceneDir);
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

void Game::BackupCurrentScene() {
  sceneMgr_.BackupCurrentScene();
}

void Game::RestoreCurrentScene(SceneContext &ctx) {
  sceneMgr_.RestoreCurrentScene(ctx);
}

void Game::DrawDebugUI(SceneContext &ctx) {
#if RC_ENABLE_IMGUI
    if (ImGui::BeginMenu("Scene")) {
      auto sceneNames = sceneMgr_.GetSceneNames();
      const char *currentSceneName = CurrentSceneName().c_str();

      for (auto& name : sceneNames) {
        bool is_selected = (name == currentSceneName);
        if (ImGui::MenuItem(name.c_str(), nullptr, is_selected)) {
          RequestChange(name);
        }
      }

      ImGui::Separator();

      ImGui::InputText("##NewSceneName", newSceneNameBuf_, sizeof(newSceneNameBuf_));
      ImGui::SameLine();
      if (ImGui::MenuItem("New Scene")) {
        std::string newName(newSceneNameBuf_);
        if (!newName.empty()) {
          if (sceneMgr_.CreateNewScene(newName, kSceneDir)) {
            newSceneNameBuf_[0] = '\0';
          }
        }
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Save Current Scene", "Ctrl+S")) {
        sceneMgr_.SaveCurrentScene();
      }

      ImGui::Separator();

      if (ImGui::BeginMenu("Delete Scene")) {
        for (auto& name : sceneNames) {
          if (name == CurrentSceneName()) continue;
          if (ImGui::MenuItem(name.c_str())) {
            sceneMgr_.DeleteScene(name);
          }
        }
        ImGui::EndMenu();
      }

      ImGui::EndMenu();
    }
#endif
}
