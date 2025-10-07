#include "TitleScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void TitleScene::Update(SceneManager &sm, SceneContext &ctx) {
  // Enter または ボタンクリックで Select へ
  if (ctx.input && ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Select");
  }
  ImGui::Begin("Title");
  ImGui::Text("Title Scene");
  if (ImGui::Button("Start -> Select")) {
    sm.RequestChange("Select");
  }
  ImGui::End();
}

void TitleScene::Render(SceneContext &, ID3D12GraphicsCommandList *) {
  // 今は ImGui のみ。必要になればメッシュ描画をここに追加。
}
