#include "SelectScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"

SelectScene::~SelectScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void SelectScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)
}

void SelectScene::OnExit(SceneContext &) {
}

void SelectScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  ctx.camera->Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = ctx.camera->GetView();
  proj_ = ctx.camera->GetProjection();
  RC::SetCamera(view_, proj_, ctx.camera->GetWorldPos());

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Game");
  }
}

void SelectScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
