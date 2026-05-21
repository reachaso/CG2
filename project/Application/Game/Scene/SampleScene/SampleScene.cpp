#include "SampleScene.h"
#include "Dx12Core.h"
#include "Input/Input.h"
#include "PipelineManager.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include <future>
#include <vector>

SampleScene::~SampleScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void SampleScene::OnEnter(SceneContext &ctx) {

  // =============================
  // Camera
  // =============================

  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.35f, -15.0f},
                     RC::Vector3{0.0f, -0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  // =============================
  // Light初期化
  // =============================

  directionalLight = RC::CreateDirectionalLight();

  if (RC::DirectionalLightSource *sun =
          RC::GetDirectionalLightPtr(directionalLight)) {
    sun->SetDirection({0.0f, -1.0f, 0.2f});   // ちょいナナメ上
    sun->SetColor({1.0f, 0.95f, 0.9f, 1.0f}); // ほんのり暖色
    sun->SetIntensity(1.5f);
  }

  pointLight = RC::CreatePointLight();
  pointLight2 = RC::CreatePointLight();
  spotLight = RC::CreateSpotLight();
  spotLight2 = RC::CreateSpotLight();

  // =============================
  // リソースの非同期ロード開始
  // =============================

  // モデルとテクスチャのロードを直列で記述（内部で自動的に並列ロードされる）
  plane = RC::LoadModel("Resources/model/plane");
  blockModel = RC::LoadModel("Resources/model/block");
  model = RC::LoadModel("Resources/model/teapot");
  tx_model = RC::LoadTex("Resources/white1x1.png");
  RC::SetModelEnvironmentCoefficient(model,0.5f);
  terrain = RC::LoadModel("Resources/model/terrain");
  tx_Skydome_ = RC::LoadTex("Resources/sky_sphere.png");
  tx_ball = RC::LoadTex("Resources/monsterBall.png");
  sprite = RC::LoadSprite("Resources/uvChecker.png", ctx);

  skybox = RC::CreateSkyBox("Resources/Skybox/skybox.dds");
  skyboxT_ = RC::GetSkyBoxTransformPtr(skybox);

  // =============================
  // 各オブジェクトへの流し込み
  // =============================

  planeTransform_ = RC::GetModelTransformPtr(plane);

  RC::SetModelColor(blockModel, blockColor_); // ちょい青で透明

  terrainT_ = RC::GetModelTransformPtr(terrain);
  terrainT_->translation.y = -1.0f;

  // 天球
  skydome = RC::GenerateSkydomeEx(tx_Skydome_, 40.0f);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydome);
  RC::SetSkydomeColor(skydome, {0.6f, 1.0f, 1.0f, 1.0f});

  primitiveSphere = RC::GenerateSphere(1.0f, tx_ball);
  primitiveSphereT_ = RC::GetPrimitiveMeshTransformPtr(primitiveSphere);
  primitiveSphereT_->rotation.y = -1.6f;

  testBox = RC::GenerateBox(2.0f, 2.0f, 2.0f, tx_model);
  RC::GetPrimitiveMeshTransformPtr(testBox)->translation = {5, 1, 0};

  testPlane = RC::GeneratePlane(10.0f, 10.0f, tx_model);
  RC::GetPrimitiveMeshTransformPtr(testPlane)->translation = {0, -0.5f, 0};

  testCylinder = RC::GenerateCylinder(0.8f, 2.5f, tx_model);
  RC::GetPrimitiveMeshTransformPtr(testCylinder)->translation = {-4, 1.25f, 0};

  testCone = RC::GenerateCone(0.8f, 2.0f, tx_model);
  RC::GetPrimitiveMeshTransformPtr(testCone)->translation = {-8, 1.0f, 0};

  testTorus = RC::GenerateTorus(1.0f, 0.3f, tx_model);
  RC::GetPrimitiveMeshTransformPtr(testTorus)->translation = {-12, 1.0f, 0};

  testCapsule = RC::GenerateCapsule(0.6f, 2.5f, tx_model);
  RC::GetPrimitiveMeshTransformPtr(testCapsule)->translation = {9, 1.25f, 0};

  RC::SetSpriteTransform(sprite, spriteTransform_);
  RC::SetSpriteScreenSize(sprite, spriteSize_.x, spriteSize_.y);

  // ===== AnimatedCube =====
  animatedCube_ = RC::LoadModel("Resources/model/AnimatedCube/AnimatedCube.gltf");
  RC::AttachModelAnimation(animatedCube_);

  // ===== Skeletonテストモデル =====
  walkModel_ = RC::LoadModel("Resources/model/human/walk.gltf");
  RC::AttachModelAnimation(walkModel_);

  simpleSkinModel_ = RC::LoadModel("Resources/model/simpleSkin");
  RC::AttachModelAnimation(simpleSkinModel_);
  // simpleSkinはwalkの右側に配置
  if (auto* t = RC::GetModelTransformPtr(simpleSkinModel_)) {
    t->translation = {3.0f, 0.0f, 0.0f};
  }
}

void SampleScene::OnExit(SceneContext &) {

  RC::UnloadModel(plane);
  plane = -1;
  RC::UnloadModel(model);
  model = -1;
  RC::UnloadModel(blockModel);
  blockModel = -1;

  RC::UnloadModel(terrain);
  terrain = -1;

  RC::UnloadSkydome(skydome);
  skydome = -1;

  RC::UnloadSprite(sprite);
  sprite = -1;

  RC::UnloadPrimitiveMesh(primitiveSphere);
  primitiveSphere = -1;

  RC::UnloadPrimitiveMesh(testBox);
  testBox = -1;

  RC::UnloadPrimitiveMesh(testPlane);
  testPlane = -1;

  RC::UnloadPrimitiveMesh(testCylinder);
  testCylinder = -1;

  RC::UnloadPrimitiveMesh(testCone);
  testCone = -1;

  RC::UnloadPrimitiveMesh(testTorus);
  testTorus = -1;

  RC::UnloadPrimitiveMesh(testCapsule);
  testCapsule = -1;

  RC::DestroyDirectionalLight(directionalLight);
  directionalLight = -1;
  RC::DestroyPointLight(pointLight);
  pointLight = -1;
  RC::DestroyPointLight(pointLight2);
  pointLight2 = -1;
  RC::DestroySpotLight(spotLight);
  spotLight = -1;
  RC::DestroySpotLight(spotLight2);
  spotLight2 = -1;

  if (animatedCube_ != -1) {
    RC::UnloadModel(animatedCube_);
    animatedCube_ = -1;
  }

  if (walkModel_ != -1) {
    RC::UnloadModel(walkModel_);
    walkModel_ = -1;
  }
  if (simpleSkinModel_ != -1) {
    RC::UnloadModel(simpleSkinModel_);
    simpleSkinModel_ = -1;
  }
}

void SampleScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ===========================================
  // ImGui
  // ===========================================

#if RC_ENABLE_IMGUI

  DrawImGui();
  RC::SetModelColor(blockModel, blockColor_);

  camera_.DrawImGui();

#endif // _DEBUG

  // ===========================================
  // 更新処理
  // ===========================================

  t += 1.0f / 60.0f;

  camera_.Update();

  planeTransform_->rotation.y += 0.01f;

  // viewとprojを渡す
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_, camera_.GetWorldPos());


  // === 天球回転 ===
  skydomeT_->rotation.y += 0.001f;

  // === AnimatedCube ===
  RC::UpdateModelAnimation(animatedCube_);

  // === Skeletonテストモデル ===
  RC::UpdateModelAnimation(walkModel_);
  RC::UpdateModelAnimation(simpleSkinModel_);
}

void SampleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // ===========================================
  // 3D描画
  // ===========================================
  RC::PreDraw3D(ctx, cl);

  // === 天球 ===
  // RC::DrawSkydome(skydome);
  RC::DrawSkyBox(skybox);

 /* RC::DrawPrimitiveMesh(primitiveSphere);
  RC::DrawPrimitiveMesh(testBox);
  RC::DrawPrimitiveMesh(testPlane);
  RC::DrawPrimitiveMesh(testCylinder);
  RC::DrawPrimitiveMesh(testCone);
  RC::DrawPrimitiveMesh(testTorus);
  RC::DrawPrimitiveMesh(testCapsule);*/

  // モデルの描画
  RC::DrawModel(plane);

  RC::DrawModel(model, tx_model);

  RC::DrawModel(terrain);

  if (animatedCube_ != -1) {
      RC::DrawModel(animatedCube_);
  }

  // === Skeletonテストモデル ===
  RC::DrawModel(walkModel_);
  RC::DrawModelSkeleton(walkModel_);

  RC::DrawModel(simpleSkinModel_);
  RC::DrawModelSkeleton(simpleSkinModel_);

  RC::DrawModelGlassTwoPass(blockModel);

  // ===========================================
  // 2D描画
  // ===========================================
  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(sprite);

  // RC::SetFogOverlayColor(fogColor_); // ちょい青
  // if (isFogEnabled_) {
  //   RC::DrawFogOverlay(t,
  //                      0.55f,                    // intensity
  //                      4.0f,                     // scale
  //                      3.5f,                     // speed
  //                      RC::Vector2{0.08f, 0.0f}, // wind
  //                      0.18f,                    // feather
  //                      0.35f                     // bottomBias
  //   );
  // }
}

#if RC_ENABLE_IMGUI
void SampleScene::DrawImGui() {

  ImGui::Begin("Debug");

  ImGui::Separator();

  if (ImGui::BeginTabBar("MainDebugTabBar")) {
    // -------------------
    // ModelTab
    // -------------------
    if (ImGui::BeginTabItem("ModelTab")) {

      RC::DrawSkyBoxImGui(skybox, "skybox");

      RC::DrawImGui3D(terrain, "terrain");

      RC::DrawImGui3D(plane, "plane");

      RC::DrawImGui3D(model, "model");

      RC::DrawSkydomeImGui(skydome, "skyDome");

      if (animatedCube_ != -1) {
          RC::DrawImGui3D(animatedCube_, "AnimatedCube");
      }

      RC::DrawImGui3D(walkModel_, "walkModel");

      RC::DrawImGui3D(simpleSkinModel_, "simpleSkinModel");

      RC::DrawImGui3D(blockModel, "blockModel");
      // 色変更用
      ImGui::ColorEdit4("blockColor", &blockColor_.x);

      ImGui::EndTabItem();
    }

    // -------------------
    // PrimitiveTab
    // -------------------
    if (ImGui::BeginTabItem("PrimitiveTab")) {

      RC::DrawPrimitiveMeshImGui(primitiveSphere, "primitiveSphere");

      RC::DrawPrimitiveMeshImGui(testBox, "testBox");

      RC::DrawPrimitiveMeshImGui(testPlane, "testPlane");

      RC::DrawPrimitiveMeshImGui(testCylinder, "testCylinder");

      RC::DrawPrimitiveMeshImGui(testCone, "testCone");

      RC::DrawPrimitiveMeshImGui(testTorus, "testTorus");

      RC::DrawPrimitiveMeshImGui(testCapsule, "testCapsule");

      ImGui::EndTabItem();
    }

    // -------------------
    // SpriteTab
    // -------------------
    if (ImGui::BeginTabItem("SpriteTab")) {

      RC::DrawImGui2D(sprite, "sprite");

      ImGui::EndTabItem();
    }

    // -------------------
    // SoundTab
    // -------------------
    if (ImGui::BeginTabItem("SoundTab")) {
      ImGui::EndTabItem();
    }

    // -------------------
    // POSTEffectTab
    // -------------------
    if (ImGui::BeginTabItem("PostEffectTab")) {

      RC::DrawPostEffectImGui();

      ImGui::Separator();

      // -------------------
      // Fog
      // -------------------
      ImGui::Checkbox("isFogEnabled", &isFogEnabled_);
      ImGui::ColorEdit4("fogColor", &fogColor_.x);

      ImGui::EndTabItem();
    }

    // -------------------
    // LightTab
    // -------------------
    if (ImGui::BeginTabItem("LightTab")) {
      RC::DrawImGuiDirectionalLight(directionalLight, "DirectionalLight");
      RC::DrawImGuiPointLight(pointLight, "PointLight");
      RC::DrawImGuiPointLight(pointLight2, "PointLight2");
      RC::DrawImGuiSpotLight(spotLight, "SpotLight");
      RC::DrawImGuiSpotLight(spotLight2, "SpotLight2");
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}
#endif
