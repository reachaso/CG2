#include "SelectScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void SelectScene::Update(SceneManager &sm, SceneContext &ctx) {
  if (ctx.input && ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Game");
  }
  ImGui::Begin("Select");
  ImGui::Text("Select Scene");
  if (ImGui::Button("Go -> Game")) {
    sm.RequestChange("Game");
  }
  if (ImGui::Button("<- Back Title")) {
    sm.RequestChange("Title");
  }
  ImGui::End();
}

void SelectScene::Render(SceneContext &, ID3D12GraphicsCommandList *) {}
