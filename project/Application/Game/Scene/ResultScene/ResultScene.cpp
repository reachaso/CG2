#include "ResultScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "Graphics/PostProcess/PostProcess.h"

ResultScene::~ResultScene() {
  SceneContext dummy{};
  OnExit(dummy);
}
#include "imgui/imgui.h"

void ResultScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)

  // ======= スカイドーム生成 =======
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kFarZ = 1000.0f;
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSkydomeEx(txSphere_, kSkyRadius);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydomeModel);
  RC::SetSkydomeColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  clearModel = RC::LoadModel("Resources/UI/Clear.obj");
  clearModelT_ = RC::GetModelTransformPtr(clearModel);
  tx_white = RC::LoadTex("Resources/white1x1.png");
  RC::SetModelColor(clearModel, {1.0f, 1.0f, 1.0f, 1.0f}); // 白色
  // カメラ位置(0, 5, -20)に対して、正面の距離に配置する
  if (clearModelT_) {
    clearModelT_->translation = {0.0f, 5.0f, -10.0f};
    clearModelT_->scale = {1.0f, 1.0f, 1.0f};
    clearModelT_->rotation.y = 3.14159f;
    clearModelT_->rotation.x = 1.5708f;
  }

  // ======= ポストエフェクト =======
  if (ctx.postProcess) {
    ctx.postProcess->AddEffect(PostEffectType::Sepia);
    ctx.postProcess->AddEffect(PostEffectType::GaussianFilter);
  }
}

void ResultScene::OnExit(SceneContext &ctx) {
  if (ctx.postProcess) {
    ctx.postProcess->ClearEffects();
  }
  
  RC::UnloadModel(clearModel);
}

void ResultScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  ctx.camera->Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = ctx.camera->GetView();
  proj_ = ctx.camera->GetProjection();
  RC::SetCamera(view_, proj_, ctx.camera->GetWorldPos());

  // ======= スカイドーム更新 =======
  if (skydomeT_) {
    // カメラ座標に追従
    skydomeT_->translation = ctx.camera->GetWorldPos();
    // 高さオフセット
    skydomeT_->translation.y -= 10.0f;
    // 自転処理
    skydomeT_->rotation.y += 0.0005f;
  }

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Title");
  }
}

void ResultScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::DrawSkydome(skydomeModel);

  RC::DrawModel(clearModel, tx_white);

  RC::PreDraw2D(ctx, cl);
}
