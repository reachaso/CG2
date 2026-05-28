#include "ResultScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"

ResultScene::~ResultScene() {
  SceneContext dummy{};
  OnExit(dummy);
}
#include "imgui/imgui.h"

void ResultScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)
}

void ResultScene::OnExit(SceneContext &ctx) {}

void ResultScene::Update(SceneManager &sm, SceneContext &ctx) {
  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Title");
  }
}

void ResultScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);

}
