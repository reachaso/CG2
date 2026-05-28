#include "GameOverScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"

GameOverScene::~GameOverScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void GameOverScene::OnEnter(SceneContext &ctx) {}

void GameOverScene::OnExit(SceneContext &ctx) {}

void GameOverScene::Update(SceneManager &sm, SceneContext &ctx) {
  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Title");
  }
}

void GameOverScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);
  RC::PreDraw2D(ctx, cl);
}
