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
