#include "TitleScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void TitleScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)
}

void TitleScene::OnExit(SceneContext &) {
}

TitleScene::~TitleScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void TitleScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  ctx.camera->Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = ctx.camera->GetView();
  proj_ = ctx.camera->GetProjection();
  RC::SetCamera(view_, proj_, ctx.camera->GetWorldPos());

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Select");
  }
}

void TitleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {

  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
