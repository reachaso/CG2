#include "LightScene.h"
#include "Dx12/Dx12Core.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void LightScene::OnEnter(SceneContext &ctx) {
  // カメラは SceneManager が所有 (ctx.camera)

  terrain_ = RC::LoadModel("Resources/model/terrain");
  terrainT_ = RC::GetModelTransformPtr(terrain_);
  terrainT_->translation.y = -1.0f;

  ball_ = RC::LoadModel("Resources/model/monsterBall");
  ballT_ = RC::GetModelTransformPtr(ball_);
  ballT_->rotation.y = -1.6f;

  plane_gltf_ = RC::LoadModel("Resources/model/plane_glTF/plane.gltf");
  planeT_ = RC::GetModelTransformPtr(plane_gltf_);
  planeT_->rotation.y = -3.5f;
  planeT_->translation = {-3.0f, 2.0f, 5.0f};

  DirectionalLight_ = RC::CreateDirectionalLight();

  PointLight1_ = RC::CreatePointLight();
  pl1_source_ = RC::GetPointLightPtr(PointLight1_);
  pl1_source_->SetPosition({-4.0f, 2.0f, -2.0f});
  pl1_source_->SetIntensity(3.0f);
  pl1_source_->SetDecay(2.0f);

  PointLight2_ = RC::CreatePointLight();
  pl2_source_ = RC::GetPointLightPtr(PointLight2_);
  pl2_source_->SetPosition({4.0f, 2.0f, -2.0f});
  pl2_source_->SetIntensity(3.0f);
  pl2_source_->SetDecay(2.0f);

  SpotLight1_ = RC::CreateSpotLight();
  sl1_source_ = RC::GetSpotLightPtr(SpotLight1_);
  sl1_source_->SetPosition({-4.0f, 2.0f, -2.0f});
  sl1_source_->SetIntensity(3.0f);
  sl1_source_->SetDistance(20.0f);
  sl1_source_->SetDecay(2.0f);
  sl1_source_->SetAngleDeg(60.0f);

  SpotLight2_ = RC::CreateSpotLight();
  sl2_source_ = RC::GetSpotLightPtr(SpotLight2_);
  sl2_source_->SetPosition({4.0f, 2.0f, -2.0f});
  sl2_source_->SetIntensity(3.0f);
  sl2_source_->SetDistance(20.0f);
  sl2_source_->SetDecay(2.0f);
  sl2_source_->SetAngleDeg(60.0f);

  AreaLight1_ = RC::CreateAreaLight();
  al1_source_ = RC::GetAreaLightPtr(AreaLight1_);
  al1_source_->SetPosition({0.0f, 2.0f, 0.0f});
  al1_source_->SetIntensity(1.0f);
  al1_source_->SetBasis({4.0f, 1.0f, 4.0f}, {-7.0f, 0.0f, 0.0f});
  al1_source_->SetSize(6.0f, 6.0f);
  al1_source_->SetRange(8.0f);
  al1_source_->SetDecay(0.0f);

  AreaLight2_ = RC::CreateAreaLight();
  al2_source_ = RC::GetAreaLightPtr(AreaLight2_);
  al2_source_->SetPosition({0.0f, 2.0f, -4.0f});
  al2_source_->SetIntensity(1.0f);
  al2_source_->SetBasis({-0.3f, 0.0f, 0.7f}, {-12.0f, 0.0f, 0.5f});
  al2_source_->SetSize(5.0f, 4.0f);
  al2_source_->SetRange(5.0f);
  al2_source_->SetDecay(0.0f);
}

void LightScene::OnExit(SceneContext &ctx) {

  RC::UnloadModel(ball_);
  ball_ = -1;
  RC::UnloadModel(terrain_);
  terrain_ = -1;
  RC::DestroyDirectionalLight(DirectionalLight_);
  DirectionalLight_ = -1;
  RC::DestroyPointLight(PointLight1_);
  PointLight1_ = -1;
  RC::DestroyPointLight(PointLight2_);
  PointLight2_ = -1;
  RC::DestroySpotLight(SpotLight1_);
  SpotLight1_ = -1;
  RC::DestroySpotLight(SpotLight2_);
  SpotLight2_ = -1;
  RC::DestroyAreaLight(AreaLight1_);
  AreaLight1_ = -1;
  RC::DestroyAreaLight(AreaLight2_);
  AreaLight2_ = -1;
}

void LightScene::Update(SceneManager &sm, SceneContext &ctx) {

  DrawImGui();
  ctx.camera->DrawImGui();

  // ===========================================
  // 更新処理
  // ===========================================

  if (ctx.isPlaying()) {
    // ==============================
    // キー入力でライト操作
    // ==============================
    Input *input = ctx.input;

    // Directional Light ON/OFF
    if (input->IsKeyTrigger(DIK_1)) {
      if (RC::IsActiveDirectionalLightEnabled()) {
        RC::SetActiveDirectionalLightEnabled(false);
      } else {
        RC::SetActiveDirectionalLightEnabled(true);
      }
    }

    // Point Light1 ON/OFF
    if (input->IsKeyTrigger(DIK_2)) {
      if (RC::IsPointLightEnabled(PointLight1_)) {
        RC::SetPointLightEnabled(PointLight1_, false);
      } else {
        RC::SetPointLightEnabled(PointLight1_, true);
      }
    }

    // Point Light2 ON/OFF
    if (input->IsKeyTrigger(DIK_3)) {
      if (RC::IsPointLightEnabled(PointLight2_)) {
        RC::SetPointLightEnabled(PointLight2_, false);
      } else {
        RC::SetPointLightEnabled(PointLight2_, true);
      }
    }

    // Spot Light1 ON/OFF
    if (input->IsKeyTrigger(DIK_4)) {
      if (RC::IsSpotLightEnabled(SpotLight1_)) {
        RC::SetSpotLightEnabled(SpotLight1_, false);
      } else {
        RC::SetSpotLightEnabled(SpotLight1_, true);
      }
    }

    // Spot Light2 ON/OFF
    if (input->IsKeyTrigger(DIK_5)) {
      if (RC::IsSpotLightEnabled(SpotLight2_)) {
        RC::SetSpotLightEnabled(SpotLight2_, false);
      } else {
        RC::SetSpotLightEnabled(SpotLight2_, true);
      }
    }

    // Area Light1 ON/OFF
    if (input->IsKeyTrigger(DIK_6)) {
      if (RC::IsAreaLightEnabled(AreaLight1_)) {
        RC::SetAreaLightEnabled(AreaLight1_, false);
      } else {
        RC::SetAreaLightEnabled(AreaLight1_, true);
      }
    }

    // Area Light2 ON/OFF
    if (input->IsKeyTrigger(DIK_7)) {
      if (RC::IsAreaLightEnabled(AreaLight2_)) {
        RC::SetAreaLightEnabled(AreaLight2_, false);
      } else {
        RC::SetAreaLightEnabled(AreaLight2_, true);
      }
    }

    // wasdでボールのscale変更
    if (input->IsKeyPressed(DIK_UP)) {
      ballT_->scale.y += 0.01f;
    }
    if (input->IsKeyPressed(DIK_LEFT)) {
      ballT_->scale.z -= 0.01f;
    }
    if (input->IsKeyPressed(DIK_DOWN)) {
      ballT_->scale.y -= 0.01f;
    }
    if (input->IsKeyPressed(DIK_RIGHT)) {
      ballT_->scale.z += 0.01f;
    }
  }
}

void LightScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  //ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);

  RC::DrawModel(plane_gltf_);

  RC::DrawModel(ball_);

  RC::DrawModel(terrain_);

  RC::PreDraw2D(ctx, cl);
}

void LightScene::DrawImGui() {
#if RC_ENABLE_IMGUI



  ImGui::Begin("Debug");

  if (ImGui::BeginTabBar("MainDebugTabBar")) {
    // -------------------
    // ModelTab
    // -------------------
    if (ImGui::BeginTabItem("ModelTab")) {

      RC::DrawImGui3D(plane_gltf_, "Plane_glTF");

      RC::DrawImGui3D(ball_, "Ball_obj");

      RC::DrawImGui3D(terrain_, "Terrain_obj");

      ImGui::EndTabItem();
    }

    // -------------------
    // LightTab
    // -------------------
    if (ImGui::BeginTabItem("LightTab")) {

      RC::DrawImGuiDirectionalLight(DirectionalLight_, "DirectionalLight");
      RC::DrawImGuiPointLight(PointLight1_, "PointLight1");
      RC::DrawImGuiPointLight(PointLight2_, "PointLight2");
      RC::DrawImGuiSpotLight(SpotLight1_, "SpotLight1");
      RC::DrawImGuiSpotLight(SpotLight2_, "SpotLight2");
      RC::DrawImGuiAreaLight(AreaLight1_, "AreaLight1");
      RC::DrawImGuiAreaLight(AreaLight2_, "AreaLight2");

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();

#endif
}
