#include "GameScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
  if (ctx.input && ctx.input->IsKeyTrigger(DIK_ESCAPE)) {
    sm.RequestChange("Result");
  }
  if (ctx.input && ctx.input->IsKeyTrigger(DIK_BACK)) {
    sm.RequestChange("Select");
  }
  ImGui::Begin("Game");
  ImGui::Text("Game Scene");
  if (ImGui::Button("Finish -> Result")) {
    sm.RequestChange("Result");
  }
  if (ImGui::Button("<- Back Select")) {
    sm.RequestChange("Select");
  }
  ImGui::End();


  
}

void GameScene::Render(SceneContext &, ID3D12GraphicsCommandList *) {}
