#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"

GameScene::~GameScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void GameScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)
}

void GameScene::OnExit(SceneContext &) {
}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
#if RC_ENABLE_IMGUI
  ctx.camera->DrawImGui();
#endif

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  ctx.camera->Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = ctx.camera->GetView();
  proj_ = ctx.camera->GetProjection();
  RC::SetCamera(view_, proj_, ctx.camera->GetWorldPos());
}

void GameScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // ==============
  // 描画処理
  // ==============

  // ======= 3D描画 =======
  RC::PreDraw3D(ctx, cl);

  // ======= 2D描画準備 =======
  RC::PreDraw2D(ctx, cl);

}
