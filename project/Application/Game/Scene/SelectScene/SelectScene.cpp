#include "SelectScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"

SelectScene::~SelectScene() {
  SceneContext dummy{};
  OnExit(dummy);
}
#include "StageSelection/StageSelection.h"
#include "imgui/imgui.h"
#include <cmath>
#include <numbers>

namespace {
static constexpr const char *kStageCsvPaths[SelectScene::kStageMax] = {
    "Resources/Stage/Stage1/stage1.csv", "Resources/Stage/Stage2/stage2.csv",
    "Resources/Stage/Stage3/stage3.csv", "Resources/Stage/Stage4/stage4.csv",
    "Resources/Stage/Stage5/stage5.csv", "Resources/Stage/Stage6/stage6.csv",
};

static constexpr float kStageRingRadius = 7.0f;
static constexpr float kStageRingHeight = 5.0f;
static constexpr float kStageRingRotateSpeed = 4.0f;
static constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
static constexpr float kGuideOffsetX = 2.0f;
static constexpr float kRingStopEpsilon = 0.001f;

static void UpdateStageRing(Transform *stageTransforms[], float ringAngle) {
  const float step = kTwoPi / static_cast<float>(SelectScene::kStageMax);

  for (int i = 0; i < SelectScene::kStageMax; ++i) {
    Transform *t = stageTransforms[i];
    if (!t) {
      continue;
    }

    const float angle = ringAngle + step * i;
    const float x = std::sin(angle) * kStageRingRadius;
    const float z = -std::cos(angle) * kStageRingRadius;

    t->translation = {x, kStageRingHeight, z};
  }
}
} // namespace

void SelectScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)

  // ======= スカイドーム生成 =======
  const float kFarZ = 1000.0f;
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSkydomeEx(txSphere_, kSkyRadius);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydomeModel);
  RC::SetSkydomeColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  // ======= ステージ選択用モデル読み込み =======
  stageModels[0] = RC::LoadModel("Resources/model/stage1");
  stageModels[1] = RC::LoadModel("Resources/model/stage2");
  stageModels[2] = RC::LoadModel("Resources/model/stage3");
  stageModels[3] = RC::LoadModel("Resources/model/stage4");
  stageModels[4] = RC::LoadModel("Resources/model/stage5");
  stageModels[5] = RC::LoadModel("Resources/model/stage6");

  stageTransforms[0] = RC::GetModelTransformPtr(stageModels[0]);
  stageTransforms[1] = RC::GetModelTransformPtr(stageModels[1]);
  stageTransforms[2] = RC::GetModelTransformPtr(stageModels[2]);
  stageTransforms[3] = RC::GetModelTransformPtr(stageModels[3]);
  stageTransforms[4] = RC::GetModelTransformPtr(stageModels[4]);
  stageTransforms[5] = RC::GetModelTransformPtr(stageModels[5]);

  const float step = kTwoPi / static_cast<float>(kStageMax);
  ringAngle_ = -static_cast<float>(selectStage) * step;
  ringTargetAngle_ = ringAngle_;
  UpdateStageRing(stageTransforms, ringAngle_);

  AModel = RC::LoadModel("Resources/model/A");
  ATransform = RC::GetModelTransformPtr(AModel);
  ATransform->scale = {0.5f, 0.5f, 0.5f};

  DModel = RC::LoadModel("Resources/model/D");
  DTransform = RC::GetModelTransformPtr(DModel);
  DTransform->scale = {0.5f, 0.5f, 0.5f};
}

void SelectScene::OnExit(SceneContext &) {
  for (int i = 0; i < kStageMax; ++i) {
    RC::UnloadModel(stageModels[i]);
    stageModels[i] = -1;
  }
  RC::UnloadSkydome(skydomeModel);
  skydomeModel = -1;

  RC::UnloadModel(AModel);
  AModel = -1;
  RC::UnloadModel(DModel);
  DModel = -1;
}

void SelectScene::Update(SceneManager &sm, SceneContext &ctx) {

#if RC_ENABLE_IMGUI

  ImGui::Begin("Stage Select");
  ImGui::Text("Use A/D to select stage.");
  ImGui::Text("Press SPACE to start the game.");
  ImGui::Separator();
  ImGui::Text("Selected Stage: %d", selectStage + 1);
  ImGui::End();

#endif

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

  if (ctx.input->IsKeyTrigger(DIK_A)) {
    selectStage = (selectStage + kStageMax - 1) % kStageMax;
  }
  if (ctx.input->IsKeyTrigger(DIK_D)) {
    selectStage = (selectStage + 1) % kStageMax;
  }

  const float step = kTwoPi / static_cast<float>(kStageMax);
  ringTargetAngle_ = -static_cast<float>(selectStage) * step;

  float diff = ringTargetAngle_ - ringAngle_;
  if (diff > std::numbers::pi_v<float>) {
    diff -= kTwoPi;
  } else if (diff < -std::numbers::pi_v<float>) {
    diff += kTwoPi;
  }

  const float maxStep = kStageRingRotateSpeed * dt;
  if (std::abs(diff) <= maxStep) {
    ringAngle_ = ringTargetAngle_;
  } else {
    ringAngle_ += (diff > 0.0f) ? maxStep : -maxStep;
  }

  UpdateStageRing(stageTransforms, ringAngle_);

  isRingRotating_ =
      std::abs(ringTargetAngle_ - ringAngle_) > kRingStopEpsilon;

  Transform *centerStage = stageTransforms[selectStage];
  if (centerStage && ATransform && DTransform) {
    const auto base = centerStage->translation;
    ATransform->translation = {base.x - kGuideOffsetX, base.y, base.z};
    DTransform->translation = {base.x + kGuideOffsetX, base.y, base.z};
  }

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    StageSelection::SetCsvPath(kStageCsvPaths[selectStage]);
    sm.RequestChange("Game");
  }
}

void SelectScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::DrawSkydome(skydomeModel);

  for (int i = 0; i < kStageMax; ++i) {
    RC::DrawModel(stageModels[i]);
  }

  if (!isRingRotating_) {
    RC::DrawModel(AModel);
    RC::DrawModel(DModel);
  }

  RC::PreDraw2D(ctx, cl);
}
