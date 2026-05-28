#include "GameOverScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "Graphics/PostProcess/PostProcess.h"

GameOverScene::~GameOverScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void GameOverScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)

  // ======= スカイドーム生成 =======
  const float kFarZ = 1000.0f;
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSkydomeEx(txSphere_, kSkyRadius);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydomeModel);
  RC::SetSkydomeColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  gameOverModel = RC::LoadModel("Resources/UI/GameOver.obj");
  gameOverModelT_ = RC::GetModelTransformPtr(gameOverModel);
  tx_white = RC::LoadTex("Resources/white1x1.png");
  RC::SetModelColor(gameOverModel, {1.0f, 0.2f, 0.2f, 1.0f});
  if (gameOverModelT_) {
    gameOverModelT_->translation = {0.0f, 5.0f, -10.0f};
    gameOverModelT_->scale = {1.0f, 1.0f, 1.0f};
    gameOverModelT_->rotation.y = 3.14159f;
    gameOverModelT_->rotation.x = 1.5708f;
  }

  // ======= ポストエフェクト =======
  if (ctx.postProcess) {
    ctx.postProcess->AddEffect(PostEffectType::Grayscale);
    ctx.postProcess->AddEffect(PostEffectType::GaussianFilter);
    ctx.postProcess->AddEffect(PostEffectType::RandomNoise);
  }

  noiseIntensity_ = 0.0f;
  RC::SetRandomNoiseIntensity(noiseIntensity_);
  RC::SetRandomNoiseColor(1.0f, 1.0f, 1.0f);
}

void GameOverScene::OnExit(SceneContext &ctx) {
  if (ctx.postProcess) {
    ctx.postProcess->ClearEffects();
  }
  
  RC::UnloadModel(gameOverModel);
}

void GameOverScene::Update(SceneManager &sm, SceneContext &ctx) {
  // 徐々にノイズを強くする演出
  const float dt = 1.0f / 60.0f;
  noiseIntensity_ += dt * 0.2f;
  if (noiseIntensity_ > 0.85f) noiseIntensity_ = 0.85f;
  RC::SetRandomNoiseIntensity(noiseIntensity_);

  // ======= スカイドーム更新 =======
  if (skydomeT_) {
    skydomeT_->translation = ctx.camera->GetWorldPos();
    skydomeT_->translation.y -= 10.0f;
    skydomeT_->rotation.y += 0.0005f;
  }

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Title");
  }
}

void GameOverScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::DrawSkydome(skydomeModel);
  RC::DrawModel(gameOverModel, tx_white);

  RC::PreDraw2D(ctx, cl);
}
