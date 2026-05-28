#include "SampleScene.h"
#include "Dx12Core.h"
#include "Input/Input.h"
#include "PipelineManager.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include "ECS/Entity.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/SkyboxComponent.h"
#include "ECS/SkydomeComponent.h"
#include "ECS/LightComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/AnimationComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/SpriteRendererComponent.h"
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
  // 非再生時はデバッグカメラ（Editorカメラ）をデフォルトで有効に
  camera_.SetUseDebug(true);

  // メインカメラのEntity化
  auto camEntity = CreateEntity("MainCamera");
  auto& camTr = camEntity->AddComponent<TransformComponent>();
  camTr.position = {0.0f, 0.35f, -15.0f};
  auto& camComp = camEntity->AddComponent<CameraComponent>();
  camComp.fovY = 0.45f;
  camComp.nearZ = 0.1f;
  camComp.farZ = 100.0f;

  // =============================
  // Light初期化
  // =============================

  // Directional Light Entity
  auto dirLightEntity = CreateEntity("DirectionalLight");
  dirLightEntity->AddComponent<TransformComponent>();
  auto& dirLightComp = dirLightEntity->AddComponent<DirectionalLightComponent>();
  dirLightComp.lightHandle = RC::CreateDirectionalLight();
  dirLightComp.direction = {0.0f, -1.0f, 0.2f};
  dirLightComp.color = {1.0f, 0.95f, 0.9f, 1.0f};
  dirLightComp.intensity = 1.5f;

  // Point Light Entity 1
  auto ptLightEntity = CreateEntity("PointLight");
  auto& ptTr = ptLightEntity->AddComponent<TransformComponent>();
  ptTr.position = {0.0f, 2.0f, 0.0f};
  auto& ptLightComp = ptLightEntity->AddComponent<PointLightComponent>();
  ptLightComp.lightHandle = RC::CreatePointLight();
  ptLightComp.color = {1.0f, 0.2f, 0.2f, 1.0f};
  ptLightComp.radius = 5.0f;

  // Point Light Entity 2
  auto ptLightEntity2 = CreateEntity("PointLight2");
  auto& ptTr2 = ptLightEntity2->AddComponent<TransformComponent>();
  ptTr2.position = {5.0f, 2.0f, 0.0f};
  auto& ptLightComp2 = ptLightEntity2->AddComponent<PointLightComponent>();
  ptLightComp2.lightHandle = RC::CreatePointLight();
  ptLightComp2.color = {0.2f, 1.0f, 0.2f, 1.0f};
  ptLightComp2.radius = 5.0f;

  // Spot Light Entity 1
  auto spLightEntity = CreateEntity("SpotLight");
  auto& spTr = spLightEntity->AddComponent<TransformComponent>();
  spTr.position = {0.0f, 5.0f, 0.0f};
  auto& spLightComp = spLightEntity->AddComponent<SpotLightComponent>();
  spLightComp.lightHandle = RC::CreateSpotLight();
  spLightComp.color = {0.2f, 0.2f, 1.0f, 1.0f};

  // Spot Light Entity 2
  auto spLightEntity2 = CreateEntity("SpotLight2");
  auto& spTr2 = spLightEntity2->AddComponent<TransformComponent>();
  spTr2.position = {5.0f, 5.0f, 0.0f};
  auto& spLightComp2 = spLightEntity2->AddComponent<SpotLightComponent>();
  spLightComp2.lightHandle = RC::CreateSpotLight();
  spLightComp2.color = {1.0f, 1.0f, 0.2f, 1.0f};

  // Area Light Entity
  auto arLightEntity = CreateEntity("AreaLight");
  auto& arTr = arLightEntity->AddComponent<TransformComponent>();
  arTr.position = {0.0f, 3.0f, 5.0f};
  auto& arLightComp = arLightEntity->AddComponent<AreaLightComponent>();
  arLightComp.lightHandle = RC::CreateAreaLight();
  arLightComp.color = {1.0f, 0.5f, 0.0f, 1.0f};
  arLightComp.halfWidth = 2.0f;
  arLightComp.halfHeight = 1.0f;

  // =============================
  // リソースの非同期ロード開始
  // =============================
  tx_model = RC::LoadTex("Resources/white1x1.png");

  // Entity化して管理するモデルたち
  auto planeEntity = CreateEntity("Plane");
  planeEntity->AddComponent<TransformComponent>();
  auto& planeRen = planeEntity->AddComponent<ModelRendererComponent>();
  planeRen.modelHandle = RC::LoadModel("Resources/model/plane");

  auto blockEntity = CreateEntity("Block");
  blockEntity->AddComponent<TransformComponent>();
  auto& blockRen = blockEntity->AddComponent<ModelRendererComponent>();
  blockRen.modelHandle = RC::LoadModel("Resources/model/block");
  blockRen.color = {0.8f, 0.9f, 1.0f, 0.12f}; // ちょい青で透明

  auto teapotEntity = CreateEntity("Teapot");
  teapotEntity->AddComponent<TransformComponent>();
  auto& teapotRen = teapotEntity->AddComponent<ModelRendererComponent>();
  teapotRen.modelHandle = RC::LoadModel("Resources/model/teapot");
  teapotRen.texOverride = tx_model;
  teapotRen.environmentCoeff = 0.5f;

  auto terrainEntity = CreateEntity("Terrain");
  auto& terrainTr = terrainEntity->AddComponent<TransformComponent>();
  terrainTr.position.y = -1.0f;
  auto& terrainRen = terrainEntity->AddComponent<ModelRendererComponent>();
  terrainRen.modelHandle = RC::LoadModel("Resources/model/terrain");

  // テクスチャロード
  tx_ball = RC::LoadTex("Resources/monsterBall.png");

  // MultiMesh の Entity化
  auto multiMeshEntity = CreateEntity("MultiMesh");
  multiMeshEntity->AddComponent<TransformComponent>();
  auto& multiMeshRen = multiMeshEntity->AddComponent<ModelRendererComponent>();
  multiMeshRen.modelHandle = RC::LoadModel("Resources/model/multiMesh");

  // Skybox の Entity化
  auto skyboxEntity = CreateEntity("Skybox");
  auto& skyboxTr = skyboxEntity->AddComponent<TransformComponent>();
  skyboxTr.scale = {100.0f, 100.0f, 100.0f}; // nearZでクリップされないよう大きくする
  auto& skyboxComp = skyboxEntity->AddComponent<SkyboxComponent>();
  skyboxComp.skyboxHandle = RC::CreateSkyBox("Resources/Skybox/skybox.dds");
  skyboxComp.color = {1.0f, 1.0f, 1.0f, 1.0f};

  // Skydome の Entity化
  auto skydomeEntity = CreateEntity("Skydome");
  skydomeEntity->AddComponent<TransformComponent>();
  auto& skydomeComp = skydomeEntity->AddComponent<SkydomeComponent>();
  skydomeComp.texOverride = RC::LoadTex("Resources/sky_sphere.png");
  skydomeComp.skydomeHandle = RC::GenerateSkydomeEx(skydomeComp.texOverride, 40.0f);
  skydomeComp.color = {0.6f, 1.0f, 1.0f, 1.0f};
  skydomeComp.visible = false; // Skybox を見せるためデフォルトは非表示
  RC::SetSkydomeColor(skydomeComp.skydomeHandle, skydomeComp.color);

  // =============================
  // プリミティブメッシュの Entity化
  // =============================
  {
    auto e = CreateEntity("Sphere");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.rotation.y = -1.6f;
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Sphere;
    pm.meshHandle = RC::GenerateSphere(1.0f, tx_ball);
    pm.texOverride = tx_ball;
  }
  {
    auto e = CreateEntity("Box");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.position = {5, 1, 0};
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Box;
    pm.meshHandle = RC::GenerateBox(2.0f, 2.0f, 2.0f, tx_model);
    pm.texOverride = tx_model;
  }
  {
    auto e = CreateEntity("GroundPlane");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.position = {0, -0.5f, 0};
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Plane;
    pm.meshHandle = RC::GeneratePlane(10.0f, 10.0f, tx_model);
    pm.texOverride = tx_model;
  }
  {
    auto e = CreateEntity("Cylinder");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.position = {-4, 1.25f, 0};
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Cylinder;
    pm.meshHandle = RC::GenerateCylinder(0.8f, 2.5f, tx_model);
    pm.texOverride = tx_model;
  }
  {
    auto e = CreateEntity("Cone");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.position = {-8, 1.0f, 0};
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Cone;
    pm.meshHandle = RC::GenerateCone(0.8f, 2.0f, tx_model);
    pm.texOverride = tx_model;
  }
  {
    auto e = CreateEntity("Torus");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.position = {-12, 1.0f, 0};
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Torus;
    pm.meshHandle = RC::GenerateTorus(1.0f, 0.3f, tx_model);
    pm.texOverride = tx_model;
  }
  {
    auto e = CreateEntity("Capsule");
    auto& tr = e->AddComponent<TransformComponent>();
    tr.position = {9, 1.25f, 0};
    auto& pm = e->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Capsule;
    pm.meshHandle = RC::GenerateCapsule(0.6f, 2.5f, tx_model);
    pm.texOverride = tx_model;
  }

  // ===== スプライトの Entity化 =====
  {
    auto spriteEntity = CreateEntity("Sprite");
    spriteEntity->AddComponent<TransformComponent>();
    auto& sprComp = spriteEntity->AddComponent<SpriteRendererComponent>();
    sprComp.spriteHandle = RC::LoadSprite("Resources/uvChecker.png", ctx);
    sprComp.size = {100.0f, 100.0f};
    RC::SetSpriteScreenSize(sprComp.spriteHandle, sprComp.size.x, sprComp.size.y);
  }

  // ===== アニメーションモデルのEntity化 =====
  {
    auto cubeEntity = CreateEntity("AnimatedCube");
    cubeEntity->AddComponent<TransformComponent>();
    auto& cubeRen = cubeEntity->AddComponent<ModelRendererComponent>();
    cubeRen.modelHandle = RC::LoadModel("Resources/model/AnimatedCube/AnimatedCube.gltf");
    cubeEntity->AddComponent<AnimationComponent>();
  }
  {
    auto walkEntity = CreateEntity("Walk");
    walkEntity->AddComponent<TransformComponent>();
    auto& walkRen = walkEntity->AddComponent<ModelRendererComponent>();
    walkRen.modelHandle = RC::LoadModel("Resources/model/human/walk.gltf");
    auto& walkAnim = walkEntity->AddComponent<AnimationComponent>();
    walkAnim.showSkeleton = true;
  }
  {
    auto skinEntity = CreateEntity("SimpleSkin");
    auto& skinTr = skinEntity->AddComponent<TransformComponent>();
    skinTr.position = {3.0f, 0.0f, 0.0f};
    auto& skinRen = skinEntity->AddComponent<ModelRendererComponent>();
    skinRen.modelHandle = RC::LoadModel("Resources/model/simpleSkin");
    auto& skinAnim = skinEntity->AddComponent<AnimationComponent>();
    skinAnim.showSkeleton = true;
  }

  // =============================
  // GPU Particle
  // =============================
  gpuParticle_ = std::make_unique<GPUParticle>();
  gpuParticle_->Initialize(ctx);
}

void SampleScene::OnExit(SceneContext &) {
  
  // Entityに紐づいたコンポーネントの解放
  for (auto& e : entities_) {
      if (auto* ren = e->GetComponent<ModelRendererComponent>()) {
          if (ren->HasModel()) RC::UnloadModel(ren->modelHandle);
      }
      if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
          if (pm->HasMesh()) RC::UnloadPrimitiveMesh(pm->meshHandle);
      }
      if (auto* skybox = e->GetComponent<SkyboxComponent>()) {
          if (skybox->HasSkybox()) RC::UnloadSkyBox(skybox->skyboxHandle);
      }
      if (auto* skydome = e->GetComponent<SkydomeComponent>()) {
          if (skydome->HasSkydome()) RC::UnloadSkydome(skydome->skydomeHandle);
      }
      if (auto* dirLight = e->GetComponent<DirectionalLightComponent>()) {
          RC::DestroyDirectionalLight(dirLight->lightHandle);
      }
      if (auto* ptLight = e->GetComponent<PointLightComponent>()) {
          RC::DestroyPointLight(ptLight->lightHandle);
      }
      if (auto* spLight = e->GetComponent<SpotLightComponent>()) {
          RC::DestroySpotLight(spLight->lightHandle);
      }
      if (auto* arLight = e->GetComponent<AreaLightComponent>()) {
          RC::DestroyAreaLight(arLight->lightHandle);
      }
      if (auto* spr = e->GetComponent<SpriteRendererComponent>()) {
          if (spr->HasSprite()) RC::UnloadSprite(spr->spriteHandle);
      }
  }
  entities_.clear();


  // GPU Particle 解放
  gpuParticle_.reset();
}

void SampleScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ===========================================
  // ImGui
  // ===========================================

#if RC_ENABLE_IMGUI

  DrawImGui();

  camera_.DrawImGui();

#endif // _DEBUG

  // ===========================================
  // 更新処理
  // ===========================================

  t += 1.0f / 60.0f;

  camera_.Update();

  if (ctx.isPlaying()) {
      for (auto& e : entities_) {
          if (e->Name() == "Plane") {
              if (auto* tr = e->GetComponent<TransformComponent>()) {
                  tr->rotation.y += 0.01f;
              }
          }
          if (e->Name() == "Skydome") {
              if (auto* tr = e->GetComponent<TransformComponent>()) {
                  tr->rotation.y += 0.001f;
              }
          }
      }
  }

  // === AnimationComponent の更新（Transform同期より前に実行）===
  for (auto& e : entities_) {
    if (!e->IsVisible()) continue;
    auto* ren = e->GetComponent<ModelRendererComponent>();
    auto* anim = e->GetComponent<AnimationComponent>();
    if (ren && anim && ren->HasModel()) {
      // 初回のみアタッチ
      if (!anim->attached_) {
        if (anim->animationPath.empty())
          RC::AttachModelAnimation(ren->modelHandle);
        else
          RC::AttachModelAnimation(ren->modelHandle, anim->animationPath);
        anim->attached_ = true;
      }
      // 再生中のみ更新（deltaTime * speed で速度制御）
      float dt = (ctx.isPlaying() && anim->playing)
          ? ctx.deltaTime * anim->speed : 0.0f;
      RC::UpdateModelAnimation(ren->modelHandle, dt);
    }
  }

  // TransformComponent の内容を各種対象に同期
  for (auto& e : entities_) {
      if (auto* tr = e->GetComponent<TransformComponent>()) {
          if (auto* ren = e->GetComponent<ModelRendererComponent>()) {
              if (ren->HasModel()) {
                  if (auto* modelTr = RC::GetModelTransformPtr(ren->modelHandle)) {
                      *modelTr = tr->ToTransform();
                  }
                  RC::SetModelColor(ren->modelHandle, ren->color);
                  RC::SetModelEnvironmentCoefficient(ren->modelHandle, ren->environmentCoeff);
              }
          }
          if (auto* skybox = e->GetComponent<SkyboxComponent>()) {
              if (skybox->HasSkybox()) {
                  if (auto* sTr = RC::GetSkyBoxTransformPtr(skybox->skyboxHandle)) {
                      *sTr = tr->ToTransform();
                  }
                  RC::SetSkyBoxColor(skybox->skyboxHandle, skybox->color);
              }
          }
          if (auto* dirLight = e->GetComponent<DirectionalLightComponent>()) {
              if (auto* l = RC::GetDirectionalLightPtr(dirLight->lightHandle)) {
                  l->SetColor(dirLight->color);
                  l->SetDirection(dirLight->direction);
                  l->SetIntensity(dirLight->intensity);
                  RC::SetDirectionalLightEnabled(dirLight->lightHandle, dirLight->visible);
              }
          }
          if (auto* ptLight = e->GetComponent<PointLightComponent>()) {
              if (auto* l = RC::GetPointLightPtr(ptLight->lightHandle)) {
                  l->SetColor(ptLight->color);
                  l->SetPosition(tr->position);
                  l->SetIntensity(ptLight->intensity);
                  l->SetRadius(ptLight->radius);
                  l->SetDecay(ptLight->decay);
                  RC::SetPointLightEnabled(ptLight->lightHandle, ptLight->visible);
              }
          }
          if (auto* spLight = e->GetComponent<SpotLightComponent>()) {
              if (auto* l = RC::GetSpotLightPtr(spLight->lightHandle)) {
                  l->SetColor(spLight->color);
                  l->SetPosition(tr->position);
                  l->SetDirection(spLight->direction);
                  l->SetIntensity(spLight->intensity);
                  l->SetDistance(spLight->distance);
                  l->SetDecay(spLight->decay);
                  l->SetCosAngle(spLight->cosAngle);
                  RC::SetSpotLightEnabled(spLight->lightHandle, spLight->visible);
              }
          }
          if (auto* arLight = e->GetComponent<AreaLightComponent>()) {
              if (auto* l = RC::GetAreaLightPtr(arLight->lightHandle)) {
                  l->SetColor(arLight->color);
                  l->SetPosition(tr->position);
                  l->SetIntensity(arLight->intensity);
                  l->SetRange(arLight->range);
                  l->SetDecay(arLight->decay);
                  l->SetHalfSize(arLight->halfWidth, arLight->halfHeight);
                  l->SetTwoSided(arLight->twoSided);
                  // To rotate AreaLight, we can use Transform rotation (if we implement normal calculation).
                  // Currently keeping it simple.
                  RC::SetAreaLightEnabled(arLight->lightHandle, arLight->visible);
              }
          }
          if (auto* skydome = e->GetComponent<SkydomeComponent>()) {
              if (skydome->HasSkydome()) {
                  if (auto* sTr = RC::GetSkydomeTransformPtr(skydome->skydomeHandle)) {
                      *sTr = tr->ToTransform();
                  }
              }
          }
          if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
              if (pm->HasMesh()) {
                  if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                      *pmTr = tr->ToTransform();
                  }
                  RC::SetPrimitiveMeshEnvironmentCoefficient(pm->meshHandle, pm->environmentCoeff);
              }
          }
          if (auto* spr = e->GetComponent<SpriteRendererComponent>()) {
              if (spr->HasSprite()) {
                  RC::SetSpriteTransform(spr->spriteHandle, tr->ToTransform());
                  RC::SetSpriteScreenSize(spr->spriteHandle, spr->size.x, spr->size.y);
                  RC::SetSpriteColor(spr->spriteHandle, spr->color);
              }
          }
      }
      e->UpdateAll(ctx.deltaTime);
  }

  // === Play/Editor カメラ切り替え ===
  for (auto& e : entities_) {
      auto* camComp = e->GetComponent<CameraComponent>();
      auto* camTr = e->GetComponent<TransformComponent>();
      if (!camComp || !camTr || !camComp->isMain) continue;

      float aspect = float(ctx.app->width) / ctx.app->height;
      if (ctx.isPlaying()) {
          // 再生中: CameraComponentのカメラを使用
          camera_.SetUseDebug(false);
          camera_.SetMainPosition(camTr->position);
          camera_.SetMainRotation(camTr->rotation);
          camera_.SetProjection(camComp->fovY, aspect, camComp->nearZ, camComp->farZ);
      } else {
          // 非再生中: Editorカメラ（デバッグカメラ）を使用
          camera_.SetUseDebug(true);
      }
      break; // メインカメラは1つだけ
  }

  // viewとprojを渡す
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_, camera_.GetWorldPos());




  if (ctx.isPlaying()) {
    // === GPU Particle 更新 ===
    if (gpuParticle_) {
      gpuParticle_->Update(view_, proj_, 1.0f / 60.0f);
    }
  }
}

void SampleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // ===========================================
  // 3D描画
  // ===========================================
  RC::PreDraw3D(ctx, cl);

  // Entityコンポーネントを持つモデル・スカイボックス・天球の描画
  for (auto& e : entities_) {
      if (!e->IsVisible()) continue; // Hierarchy の目アイコンで非表示

      if (auto* skybox = e->GetComponent<SkyboxComponent>()) {
          if (skybox->HasSkybox() && skybox->visible) {
              RC::DrawSkyBox(skybox->skyboxHandle);
          }
      }
      if (auto* skydome = e->GetComponent<SkydomeComponent>()) {
          if (skydome->HasSkydome() && skydome->visible) {
              RC::DrawSkydome(skydome->skydomeHandle, skydome->texOverride);
          }
      }
      if (auto* ren = e->GetComponent<ModelRendererComponent>()) {
          if (ren->HasModel() && ren->visible) {
              if (e->Name() == "Block") {
                  RC::DrawModelGlassTwoPass(ren->modelHandle, ren->texOverride);
              } else {
                  RC::DrawModel(ren->modelHandle, ren->texOverride);
              }
              // スケルトンのデバッグ表示
              if (auto* anim = e->GetComponent<AnimationComponent>()) {
                  if (anim->showSkeleton) {
                      RC::DrawModelSkeleton(ren->modelHandle);
                  }
              }
          }
      }
      if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
          if (pm->HasMesh() && pm->visible) {
              RC::DrawPrimitiveMesh(pm->meshHandle, pm->texOverride);
          }
      }
  }



  // === GPU Particle 描画 ===
  if (gpuParticle_) {
    gpuParticle_->Render(ctx, cl);
  }

  // === ギズモ描画（オーバーレイ: モデルの上に常に描画） ===
  RC::BeginOverlay3D();
  DrawLightGizmos(selectedEntityId_);
  DrawCameraGizmos(selectedEntityId_, float(ctx.app->width) / ctx.app->height);
  RC::EndOverlay3D();

  // ===========================================
  // 2D描画
  // ===========================================
  RC::PreDraw2D(ctx, cl);

  // Entityコンポーネントのスプライト描画
  for (auto& e : entities_) {
      if (!e->IsVisible()) continue;
      if (auto* spr = e->GetComponent<SpriteRendererComponent>()) {
          if (spr->HasSprite() && spr->visible) {
              RC::DrawSprite(spr->spriteHandle);
          }
      }
  }

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

      camera_.DrawImGui();

      ImGui::EndTabItem();
    }

    // -------------------
    // PrimitiveTab
    // -------------------
    if (ImGui::BeginTabItem("PrimitiveTab")) {

      ImGui::EndTabItem();
    }

    // -------------------
    // SpriteTab
    // -------------------
    if (ImGui::BeginTabItem("SpriteTab")) {

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
    // ParticleTab
    // -------------------
    if (ImGui::BeginTabItem("ParticleTab")) {
      if (gpuParticle_) {
        gpuParticle_->DrawImGui();
      }
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}
#endif
