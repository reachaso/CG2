#include "TitleScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void TitleScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)

  frameCount = 0;

  // タイトルモデル読み込み
  titleModel = RC::LoadModel("Resources/model/Title/Title.obj");
  RC::SetModelLightingMode(titleModel, None);
  titleT_ = RC::GetModelTransformPtr(titleModel);

  titleT_->translation = {6.0f, 5.0f, 30.0f};
  titleT_->scale = {1.0f, 1.0f, 1.0f}; 

  // ======= スカイドーム生成 =======
  const float kFarZ = 1000.0f;
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSkydomeEx(txSphere_, kSkyRadius);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydomeModel);
  RC::SetSkydomeColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  guideSprite = RC::LoadSprite("Resources/UI/Guide.png",ctx);
  RC::SetSpriteScreenSize(guideSprite, 1280, 720);
  RC::SetSpriteColor(guideSprite, {1.0f, 1.0f, 1.0f, 0.9f});
}

void TitleScene::OnExit(SceneContext &) {
  // タイトルモデル破棄
  RC::UnloadModel(titleModel);
  titleModel = -1;

  RC::UnloadSkydome(skydomeModel);
  skydomeModel = -1;

  RC::UnloadSprite(guideSprite);
  guideSprite = -1;
}

TitleScene::~TitleScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void TitleScene::Update(SceneManager &sm, SceneContext &ctx) {

    frameCount++;

  // タイトルモデルをふわふわ動かす
  if (titleT_) {
    const float t = frameCount * 0.02f;
    titleT_->translation.y = 5.0f + std::sin(t) * 0.5f;
  }

   // ======= スカイドーム更新 =======
  if (skydomeT_) {
    // カメラ座標に追従
    skydomeT_->translation = ctx.camera->GetWorldPos();
    // 高さオフセット
    skydomeT_->translation.y -= 10.0f;
    // 自転処理
    skydomeT_->rotation.y += 0.0005f;
  }

  if (guideSprite >= 0) {
    const float t = frameCount * 0.05f;
    const float alpha = 0.35f + (std::sin(t) * 0.5f + 0.5f) * 0.55f;
    RC::SetSpriteColor(guideSprite, {1.0f, 1.0f, 1.0f, alpha});
  }


  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Select");
  }
}

void TitleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {

  RC::PreDraw3D(ctx, cl);

  RC::DrawSkydome(skydomeModel);

  RC::DrawModel(titleModel);

  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(guideSprite);
}
