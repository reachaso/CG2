#include "ResultScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void ResultScene::Update(SceneManager &sm, SceneContext &ctx) {
  if (ctx.input && (ctx.input->IsKeyTrigger(DIK_SPACE))) {
    sm.RequestChange("Title");
  }
  ImGui::Begin("Result");
  ImGui::Text("Result Scene");
  if (ImGui::Button("Back to Title")) {
    sm.RequestChange("Title");
  }
  ImGui::End();
}

void ResultScene::Render(SceneContext &, ID3D12GraphicsCommandList *) {}
