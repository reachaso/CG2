#include "GameScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include "Dx12Core.h"

void GameScene::OnEnter(SceneContext &ctx) {
  ID3D12Device *device = ctx.core->GetDevice();
  auto &srvHeap = ctx.core->SRV();

  // TextureManager 初期化
  texMgr_.Init(device, &srvHeap);

   // ===== Camera =====
  camera_.Initialize(ctx.input, Vector3{0.0f, 0.0f, -5.0f},
                     Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  // モデル初期化
  teapot = new Model3D();
  teapot->Initialize(device);
  teapot->LoadObjGeometryLikeFunction("Resources", "teapot.obj");
  tx_teapot = texMgr_.LoadID("Resources/uvChecker.png", true);
  teapot->SetTexture(texMgr_.GetSrv(tx_teapot));
}

void GameScene::OnExit(SceneContext &) {
  if (teapot) {
    delete teapot;
    teapot = nullptr;
  }
  tx_teapot = -1;
}


void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
  if (ctx.input && ctx.input->IsKeyTrigger(DIK_ESCAPE)) {
    sm.RequestChange("Result");
  }
  if (ctx.input && ctx.input->IsKeyTrigger(DIK_BACK)) {
    sm.RequestChange("Select");
  }
  ImGui::Begin("Game");
  ImGui::Text("Game Scene");
  if (ImGui::Button("Finish -> Result")) {
    sm.RequestChange("Result");
  }
  if (ImGui::Button("<- Back Select")) {
    sm.RequestChange("Select");
  }
  ImGui::End();


  camera_.DrawImGui();
  camera_.Update();
  CameraMatrices mats = camera_.GetMatrices();

  teapot->Update(mats.view, mats.proj);
}

void GameScene::Render(SceneContext &, ID3D12GraphicsCommandList *cl) {

  teapot->Draw(cl);
}
