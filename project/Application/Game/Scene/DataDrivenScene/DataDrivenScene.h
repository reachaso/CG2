#pragma once
#include "Scene.h"
#include "Common/Math/MathUtils.h"
#include "RenderCommon.h"
#include <string>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "Common/Log/Log.h"
#include "Input/Input.h"

// All component headers (for factory registration)
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/SpriteRendererComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/LightComponent.h"
#include "ECS/ColliderComponent.h"
#include "ECS/AnimationComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/SkyboxComponent.h"
#include "ECS/SkydomeComponent.h"
#include "ECS/WaterComponent.h"
#include "ECS/RigidbodyComponent.h"
#include "ECS/NativeScriptComponent.h"
// Light sources for Dereferencing
#include "Graphics/Light/Directional/DirectionalLightSource.h"
#include "Graphics/Light/Point/PointLightSource.h"
#include "Graphics/Light/Spot/SpotLightSource.h"
#include "Graphics/Light/Area/AreaLightSource.h"

/// @class DataDrivenScene
/// @brief Generic Scene class that builds scenes from JSON files.
/// @details Supports creation, saving, loading, and deletion from the editor.
class DataDrivenScene : public Scene {
public:
  /// @brief Constructor
  /// @param name Scene name (display name / transition key)
  /// @param filePath JSON file path
  DataDrivenScene(const std::string& name, const std::string& filePath)
    : sceneName_(name), filePath_(filePath) {
    static bool factoryInitialized = false;
    if (!factoryInitialized) {
      RegisterAllComponents();
      factoryInitialized = true;
    }
  }

  const char* Name() const override { return sceneName_.c_str(); }

  /// @brief Load entities from JSON on scene enter
  void OnEnter(SceneContext& ctx) override {
    Load();
    resultTriggered_ = false;
    resultDelayTimer_ = 0.0f;
    // Initialize runtime handles for all loaded components
    for (auto& e : entities_) {
        InitializeRuntimeResources(*e, ctx);
    }
  }

  /// @brief Clear entities on scene exit
  void OnExit(SceneContext&) override {
    for (auto& e : entities_) {
        ReleaseRuntimeResources(*e);
    }
    entities_.clear();
    pendingEntities_.clear();
  }

  void Update(SceneManager& sm, SceneContext& ctx) override {
    (void)sm;
    currentContext_ = &ctx;

    // F3 キーでコライダーデバッグ描画をトグル
    if (ctx.input && ctx.input->IsKeyTrigger(DIK_F3)) {
        showColliderGizmos_ = !showColliderGizmos_;
    }

    // NativeScriptComponent に Scene/Context 参照を設定
    for (auto& e : entities_) {
        if (auto* nsc = e->GetComponent<NativeScriptComponent>()) {
            nsc->SetScene(this);
            nsc->SetSceneContext(&ctx);
        }
    }
    
    // === AnimationComponent の更新（Transform同期より前に実行）===
    for (auto& e : entities_) {
      if (!e->IsVisible()) continue;
      auto* ren = e->GetComponent<ModelRendererComponent>();
      auto* anim = e->GetComponent<AnimationComponent>();
      if (ren && anim && ren->HasModel()) {
        if (!anim->attached_) {
          if (anim->animationPath.empty())
            RC::AttachModelAnimation(ren->modelHandle);
          else
            RC::AttachModelAnimation(ren->modelHandle, anim->animationPath);
          anim->attached_ = true;
        }
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
                        auto* anim = e->GetComponent<AnimationComponent>();
                        if (anim && anim->attached_ && !RC::HasModelSkinData(ren->modelHandle)) {
                            // Node animation case: animation controls the root transform.
                            // Read back from modelTr to TransformComponent to keep them in sync,
                            // and avoid overwriting the animation frame.
                            tr->SetFromTransform(*modelTr);
                        } else {
                            // Skeletal animation or no animation: TransformComponent controls root transform.
                            *modelTr = tr->ToTransform();
                        }
                    }
                    RC::SetModelColor(ren->modelHandle, ren->color);
                    RC::SetModelEnvironmentCoefficient(ren->modelHandle, ren->environmentCoeff);
                    RC::SetModelNormalMap(ren->modelHandle, ren->normalMapOverride);
                    RC::SetModelRoughnessMap(ren->modelHandle, ren->roughnessMapOverride);
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
                    RC::SetDirectionalLightEnabled(dirLight->lightHandle, dirLight->visible && dirLight->IsEnabled());
                }
            }
            if (auto* ptLight = e->GetComponent<PointLightComponent>()) {
                if (auto* l = RC::GetPointLightPtr(ptLight->lightHandle)) {
                    l->SetColor(ptLight->color);
                    l->SetPosition(tr->position);
                    l->SetIntensity(ptLight->intensity);
                    l->SetRadius(ptLight->radius);
                    l->SetDecay(ptLight->decay);
                    RC::SetPointLightEnabled(ptLight->lightHandle, ptLight->visible && ptLight->IsEnabled());
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
                    RC::SetSpotLightEnabled(spLight->lightHandle, spLight->visible && spLight->IsEnabled());
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
                    RC::SetAreaLightEnabled(arLight->lightHandle, arLight->visible && arLight->IsEnabled());
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
                    RC::SetPrimitiveMeshNormalMap(pm->meshHandle, pm->normalMapOverride);
                    RC::SetPrimitiveMeshRoughnessMap(pm->meshHandle, pm->roughnessMapOverride);
                }
            }
            if (auto* water = e->GetComponent<WaterComponent>()) {
                if (water->HasMesh()) {
                    if (auto* wTr = RC::GetWaterTransformPtr(water->meshHandle)) {
                        *wTr = tr->ToTransform();
                    }
                    RC::SetWaterEnvironmentCoefficient(water->meshHandle, water->environmentCoeff);
                    RC::SetWaterParams(
                        water->waveHeight, water->waveSpeed, water->waveFreq,
                        water->waveHeight2, water->waveSpeed2, water->waveFreq2,
                        water->waveSteepness,
                        water->shallowColor, water->deepColor,
                        water->fresnelPower, water->specularPower,
                        water->normalScrollSpeed, water->normalStrength);
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
    }

    // 水面の累積時間を更新
    waterTime_ += ctx.deltaTime;
    RC::SetWaterTime(waterTime_);

    float updateDt = ctx.isPlaying() ? ctx.deltaTime : 0.0f;

    if (ctx.isPlaying()) {
        if (gameMode_ && !gameMode_->HasBegunPlay()) {
            gameMode_->BeginPlay(ctx);
            gameMode_->MarkBegunPlay();
        }
        if (gameMode_) {
            gameMode_->Tick(ctx);
        }
    }

    UpdateEntities(updateDt);
    ResolveCollisions();

    // === ゲーム結果判定（プレイ中のみ） ===
    if (ctx.isPlaying() && !resultTriggered_) {
        if (sceneName_ != "Game") {
            // ゲーム以外のシーンではスペースキーで次のシーンへ
            if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
                resultTriggered_ = true;
                if (sceneName_ == "Title") {
                    resultTarget_ = "Select";
                } else if (sceneName_ == "Select") {
                    resultTarget_ = "Game";
                } else if (sceneName_ == "Result" || sceneName_ == "GameOver") {
                    resultTarget_ = "Title";
                } else {
                    resultTarget_ = "Title";
                }
                // 即座に遷移させるためディレイを最大値にする
                resultDelayTimer_ = kResultDelay_;
            }
        } else {
            // プレイヤー死亡チェック
            for (auto& e : entities_) {
                if (e && e->GetName() == "player" && e->HasTag("game_over")) {
                    resultTriggered_ = true;
                    resultTarget_ = "GameOver";
                    resultDelayTimer_ = 0.0f;
                    break;
                }
            }
            // 全エネミー撃破チェック（プレイヤーが生きている場合のみ）
            if (!resultTriggered_) {
                bool hasEnemy = false;
                bool allDefeated = true;
                for (auto& e : entities_) {
                    if (e && (e->GetName() == "Enemy" || e->GetName() == "Shark" || e->HasTag("is_enemy"))) {
                        hasEnemy = true;
                        if (!e->HasTag("enemy_defeated")) {
                            allDefeated = false;
                            break;
                        }
                    }
                }
                if (hasEnemy && allDefeated) {
                    resultTriggered_ = true;
                    resultTarget_ = "Result";
                    resultDelayTimer_ = 0.0f;
                }
            }
        }
    }

    // デッドエンティティの削除は判定後に行う（タグを読み取れるようにするため）
    RemoveDeadEntities();

    // シーン遷移ディレイ処理
    if (resultTriggered_ && ctx.isPlaying()) {
        resultDelayTimer_ += ctx.deltaTime;
        if (resultDelayTimer_ >= kResultDelay_) {
            sm.RequestChange(resultTarget_);
            resultTriggered_ = false; // 二重リクエスト防止
        }
    }

    // === Play/Editor カメラ切り替え ===
    for (auto& e : entities_) {
        auto* camComp = e->GetComponent<CameraComponent>();
        auto* camTr = e->GetComponent<TransformComponent>();
        if (!camComp || !camTr || !camComp->isMain) continue;

        float aspect = float(ctx.app->width) / ctx.app->height;
        if (ctx.isPlaying()) {
            ctx.camera->SetUseDebug(false);
            ctx.camera->SetMainPosition(RC::Add(camTr->position, camComp->shakeOffset));
            ctx.camera->SetMainRotation(camTr->rotation);
            ctx.camera->SetProjection(camComp->fovY, aspect, camComp->nearZ, camComp->farZ);
        } else {
            ctx.camera->SetUseDebug(true);
        }
        break; // メインカメラは1つだけ
    }
  }

  void Render(SceneContext& ctx, ID3D12GraphicsCommandList* cl) override {
    // ===========================================
    // 3D描画
    // ===========================================
    RC::PreDraw3D(ctx, cl);

    // Entityコンポーネントを持つモデル・スカイボックス・天球の描画
    for (auto& e : entities_) {
        if (!e->IsVisible() || !e->IsActive()) continue; // Hierarchy の目アイコンで非表示、または非アクティブ時はスキップ

        if (auto* skybox = e->GetComponent<SkyboxComponent>()) {
            if (skybox->HasSkybox() && skybox->visible && skybox->IsEnabled()) {
                RC::DrawSkyBox(skybox->skyboxHandle);
            }
        }
        if (auto* skydome = e->GetComponent<SkydomeComponent>()) {
            if (skydome->HasSkydome() && skydome->visible && skydome->IsEnabled()) {
                RC::DrawSkydome(skydome->skydomeHandle, skydome->texOverride);
            }
        }
        if (auto* ren = e->GetComponent<ModelRendererComponent>()) {
            if (ren->HasModel() && ren->visible && ren->IsEnabled()) {
                if (e->Name() == "Block") {
                    RC::DrawModelGlassTwoPass(ren->modelHandle, ren->texOverride);
                } else {
                    RC::DrawModel(ren->modelHandle, ren->texOverride);
                }
                // スケルトンのデバッグ表示
                if (auto* anim = e->GetComponent<AnimationComponent>()) {
                    if (anim->showSkeleton && anim->IsEnabled()) {
                        RC::DrawModelSkeleton(ren->modelHandle);
                    }
                }
            }
        }
        if (auto* pm = e->GetComponent<PrimitiveMeshComponent>()) {
            if (pm->HasMesh() && pm->visible && pm->IsEnabled()) {
                std::string name = e->GetName();
                if (name == "PlayerBullet" || name == "EnemyBullet" || name == "Splash") {
                    RC::DrawPrimitiveMeshWater(pm->meshHandle, pm->texOverride);
                } else if (name == "HeavySplash") {
                    RC::DrawPrimitiveMeshWaterColumn(pm->meshHandle, pm->texOverride);
                } else {
                    RC::DrawPrimitiveMesh(pm->meshHandle, pm->texOverride);
                }
            }
        }
        if (auto* water = e->GetComponent<WaterComponent>()) {
            if (water->HasMesh() && water->visible && water->IsEnabled()) {
                RC::DrawWater(water->meshHandle, water->normalMapHandle);
            }
        }
    }

    // === ギズモ描画（オーバーレイ: モデルの上に常に描画） ===
    RC::BeginOverlay3D();
    DrawLightGizmos(selectedEntityId_);
    DrawCameraGizmos(selectedEntityId_, float(ctx.app->width) / ctx.app->height);
    DrawColliderGizmos(selectedEntityId_);
    RC::EndOverlay3D();

    // ===========================================
    // 2D描画
    // ===========================================
    RC::PreDraw2D(ctx, cl);

    // Entityコンポーネントのスプライト描画
    for (auto& e : entities_) {
        if (!e->IsVisible() || !e->IsActive()) continue;
        if (auto* spr = e->GetComponent<SpriteRendererComponent>()) {
            if (spr->HasSprite() && spr->visible && spr->IsEnabled()) {
                RC::DrawSprite(spr->spriteHandle);
            }
        }
    }

    // NativeScript の OnRender（HUD描画など、コマンドリストが開いた状態で実行）
    if (ctx.isPlaying()) {
        for (auto& e : entities_) {
            if (!e || e->IsPendingDestroy() || !e->IsActive()) continue;
            if (auto* nsc = e->GetComponent<NativeScriptComponent>()) {
                for (auto& entry : nsc->scripts) {
                    if (entry.instance) {
                        entry.instance->OnRender();
                    }
                }
            }
        }
    }
  }

  ~DataDrivenScene() override = default;

  // =================================================================
  // Save / Load
  // =================================================================

  /// @brief Save current scene state to JSON file
  bool Save() const {
    nlohmann::json root;
    root["sceneName"] = sceneName_;
    nlohmann::json entitiesJson = nlohmann::json::array();
    for (auto& e : entities_) {
      if (e) {
        entitiesJson.push_back(e->Serialize());
      }
    }
    root["entities"] = entitiesJson;

    std::filesystem::path p(filePath_);
    if (p.has_parent_path()) {
      std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream ofs(filePath_);
    if (!ofs.is_open()) {
      Log::Print("[DataDrivenScene] Failed to save: " + filePath_);
      return false;
    }
    ofs << root.dump(2);
    Log::Print("[DataDrivenScene] Saved: " + filePath_);
    return true;
  }

  /// @brief Load entities from JSON file
  bool Load() {
    entities_.clear();
    gameMode_ = std::make_unique<GameModeBase>(); // GameModeのリセット

    if (!std::filesystem::exists(filePath_)) {
      Log::Print("[DataDrivenScene] File not found: " + filePath_);
      return false;
    }

    std::ifstream ifs(filePath_);
    if (!ifs.is_open()) {
      Log::Print("[DataDrivenScene] Failed to open: " + filePath_);
      return false;
    }

    nlohmann::json root;
    try {
      ifs >> root;
    } catch (const nlohmann::json::exception& e) {
      Log::Print("[DataDrivenScene] JSON parse error: " + std::string(e.what()));
      return false;
    }

    if (root.contains("entities")) {
      for (auto& ej : root["entities"]) {
        auto entity = std::make_shared<Entity>();
        entity->Deserialize(ej);
        entities_.push_back(entity);
      }
    }

    Log::Print("[DataDrivenScene] Loaded: " + filePath_ +
               " (" + std::to_string(entities_.size()) + " entities)");
    return true;
  }

  /// @brief Get JSON file path
  const std::string& FilePath() const { return filePath_; }

  /// @brief Change scene name
  void SetSceneName(const std::string& name) { sceneName_ = name; }

  /// @brief Backup the current state of entities to memory
  void BackupState() {
      backupJson_.clear();
      for (auto& e : entities_) {
          if (e) {
              backupJson_.push_back(e->Serialize());
          }
      }
  }

  /// @brief Restore the state of entities from memory
  void RestoreState(SceneContext& ctx) {
      if (backupJson_.empty()) return;
      
      for (auto& e : entities_) {
          ReleaseRuntimeResources(*e);
      }
      entities_.clear();
      pendingEntities_.clear();
      gameMode_ = std::make_unique<GameModeBase>(); // GameModeのリセット

      for (auto& ej : backupJson_) {
          auto entity = std::make_shared<Entity>();
          entity->Deserialize(ej);
          entities_.push_back(entity);
      }
      
      for (auto& e : entities_) {
          InitializeRuntimeResources(*e, ctx);
      }
      resultTriggered_ = false;
      resultDelayTimer_ = 0.0f;
  }

  /// @brief 動的に生成したエンティティのランタイムリソースを初期化する
  void InitDynamicEntityRuntime(Entity& e) override {
      if (currentContext_) {
          InitializeRuntimeResources(e, *currentContext_);
      }
  }

  /// @brief 動的に生成したエンティティのランタイムリソースを解放する
  void ReleaseDynamicEntityRuntime(Entity& e) override {
      ReleaseRuntimeResources(e);
  }

private:
  std::string sceneName_;
  std::string filePath_;
  float waterTime_ = 0.0f; ///< 水面アニメーション用累積時間
  nlohmann::json backupJson_; ///< メモリ上へのバックアップ用

  // ゲーム結果判定用
  bool resultTriggered_ = false;      ///< 結果判定がトリガーされたか
  float resultDelayTimer_ = 0.0f;     ///< 遷移までのディレイタイマー
  std::string resultTarget_;          ///< 遷移先シーン名
  static constexpr float kResultDelay_ = 2.5f; ///< 結果確定からシーン遷移までの待機時間（秒）

  /// @brief Register all components to ComponentFactory (called once)
  static void RegisterAllComponents() {
    Entity::ComponentFactory::Register<TransformComponent>("TransformComponent");
    Entity::ComponentFactory::Register<ModelRendererComponent>("ModelRendererComponent");
    Entity::ComponentFactory::Register<SpriteRendererComponent>("SpriteRendererComponent");
    Entity::ComponentFactory::Register<CameraComponent>("CameraComponent");
    Entity::ComponentFactory::Register<DirectionalLightComponent>("DirectionalLightComponent");
    Entity::ComponentFactory::Register<PointLightComponent>("PointLightComponent");
    Entity::ComponentFactory::Register<SpotLightComponent>("SpotLightComponent");
    Entity::ComponentFactory::Register<AreaLightComponent>("AreaLightComponent");
    Entity::ComponentFactory::Register<ColliderComponent>("ColliderComponent");
    Entity::ComponentFactory::Register<AnimationComponent>("AnimationComponent");
    Entity::ComponentFactory::Register<PrimitiveMeshComponent>("PrimitiveMeshComponent");
    Entity::ComponentFactory::Register<SkyboxComponent>("SkyboxComponent");
    Entity::ComponentFactory::Register<SkydomeComponent>("SkydomeComponent");
    Entity::ComponentFactory::Register<WaterComponent>("WaterComponent");
    Entity::ComponentFactory::Register<RigidbodyComponent>("RigidbodyComponent");
    Entity::ComponentFactory::Register<NativeScriptComponent>("NativeScriptComponent");
  }

  void InitializeRuntimeResources(Entity& e, SceneContext& ctx) {
      if (auto* ren = e.GetComponent<ModelRendererComponent>()) {
          if (!ren->modelPath.empty()) ren->modelHandle = RC::LoadModel(ren->modelPath);
          if (!ren->texturePath.empty()) ren->texOverride = RC::LoadTex(ren->texturePath);
          if (!ren->normalMapPath.empty()) ren->normalMapOverride = RC::LoadTex(ren->normalMapPath);
          if (!ren->roughnessMapPath.empty()) ren->roughnessMapOverride = RC::LoadTex(ren->roughnessMapPath);
      }
      if (auto* pm = e.GetComponent<PrimitiveMeshComponent>()) {
          if (!pm->texturePath.empty()) pm->texOverride = RC::LoadTex(pm->texturePath);
          if (!pm->normalMapPath.empty()) pm->normalMapOverride = RC::LoadTex(pm->normalMapPath);
          if (!pm->roughnessMapPath.empty()) pm->roughnessMapOverride = RC::LoadTex(pm->roughnessMapPath);
          switch (pm->type) {
              case PrimitiveType::Sphere: pm->meshHandle = RC::GenerateSphere(1.0f, pm->texOverride); break;
              case PrimitiveType::Box: pm->meshHandle = RC::GenerateBox(1.0f, 1.0f, 1.0f, pm->texOverride); break;
              case PrimitiveType::Plane: pm->meshHandle = RC::GeneratePlane(10.0f, 10.0f, pm->texOverride); break;
              case PrimitiveType::Cylinder: pm->meshHandle = RC::GenerateCylinder(1.0f, 1.0f, pm->texOverride); break;
              case PrimitiveType::Cone: pm->meshHandle = RC::GenerateCone(1.0f, 1.0f, pm->texOverride); break;
              case PrimitiveType::Torus: pm->meshHandle = RC::GenerateTorus(1.0f, 0.3f, pm->texOverride); break;
              case PrimitiveType::Capsule: pm->meshHandle = RC::GenerateCapsule(0.5f, 1.0f, pm->texOverride); break;
          }
      }
      if (auto* dl = e.GetComponent<DirectionalLightComponent>()) {
          dl->lightHandle = RC::CreateDirectionalLight(RC::LightActivateMode::Add);
          if (dl->lightHandle >= 0) {
              if (auto* ptr = RC::GetDirectionalLightPtr(dl->lightHandle)) {
                  ptr->Data().color = dl->color;
                  ptr->Data().direction = dl->direction;
                  ptr->Data().intensity = dl->intensity;
              }
          }
      }
      if (auto* pl = e.GetComponent<PointLightComponent>()) {
          pl->lightHandle = RC::CreatePointLight(RC::LightActivateMode::Add);
          if (pl->lightHandle >= 0) {
              if (auto* ptr = RC::GetPointLightPtr(pl->lightHandle)) {
                  ptr->Data().color = {pl->color.x, pl->color.y, pl->color.z, pl->color.w};
                  ptr->Data().radius = pl->radius;
                  ptr->Data().intensity = pl->intensity;
                  ptr->Data().decay = pl->decay;
              }
          }
      }
      if (auto* sl = e.GetComponent<SpotLightComponent>()) {
          sl->lightHandle = RC::CreateSpotLight(RC::LightActivateMode::Add);
          if (sl->lightHandle >= 0) {
              if (auto* ptr = RC::GetSpotLightPtr(sl->lightHandle)) {
                  ptr->Data().color = {sl->color.x, sl->color.y, sl->color.z, sl->color.w};
                  ptr->Data().distance = sl->distance;
                  ptr->Data().intensity = sl->intensity;
                  ptr->Data().decay = sl->decay;
                  ptr->Data().direction = sl->direction;
                  ptr->Data().cosAngle = sl->cosAngle;
              }
          }
      }
      if (auto* al = e.GetComponent<AreaLightComponent>()) {
          al->lightHandle = RC::CreateAreaLight(RC::LightActivateMode::Add);
          if (al->lightHandle >= 0) {
              if (auto* ptr = RC::GetAreaLightPtr(al->lightHandle)) {
                  ptr->Data().color = {al->color.x, al->color.y, al->color.z, al->color.w};
                  ptr->Data().intensity = al->intensity;
                  ptr->Data().range = al->range;
                  ptr->Data().decay = al->decay;
                  ptr->Data().twoSided = al->twoSided;
                  ptr->Data().halfWidth = al->halfWidth;
                  ptr->Data().halfHeight = al->halfHeight;
              }
          }
      }
      if (auto* sb = e.GetComponent<SkyboxComponent>()) {
          if (!sb->skyboxPath.empty()) sb->skyboxHandle = RC::CreateSkyBox(sb->skyboxPath);
      }
      if (auto* sd = e.GetComponent<SkydomeComponent>()) {
          int initTex = -1;
          if (!sd->texturePath.empty()) initTex = RC::LoadTex(sd->texturePath);
          else if (!sd->skydomePath.empty()) initTex = RC::LoadTex(sd->skydomePath);
          sd->texOverride = initTex;
          sd->skydomeHandle = RC::GenerateSkydomeEx(initTex);
      }
      if (auto* spr = e.GetComponent<SpriteRendererComponent>()) {
          if (!spr->spritePath.empty()) spr->spriteHandle = RC::LoadSprite(spr->spritePath, ctx);
      }
      if (auto* water = e.GetComponent<WaterComponent>()) {
          int normalMap = -1;
          if (!water->normalMapPath.empty()) {
              normalMap = RC::LoadTex(water->normalMapPath);
              water->normalMapHandle = normalMap;
          }
          water->meshHandle = RC::GenerateWaterPlane(
              water->planeWidth, water->planeHeight, water->segments, normalMap);
      }
  }

  void ReleaseRuntimeResources(Entity& e) {
      if (auto* ren = e.GetComponent<ModelRendererComponent>()) {
          if (ren->modelHandle >= 0) { RC::UnloadModel(ren->modelHandle); ren->modelHandle = -1; }
      }
      if (auto* pm = e.GetComponent<PrimitiveMeshComponent>()) {
          if (pm->meshHandle >= 0) { RC::UnloadPrimitiveMesh(pm->meshHandle); pm->meshHandle = -1; }
      }
      if (auto* dl = e.GetComponent<DirectionalLightComponent>()) {
          if (dl->lightHandle >= 0) { RC::DestroyDirectionalLight(dl->lightHandle); dl->lightHandle = -1; }
      }
      if (auto* pl = e.GetComponent<PointLightComponent>()) {
          if (pl->lightHandle >= 0) { RC::DestroyPointLight(pl->lightHandle); pl->lightHandle = -1; }
      }
      if (auto* sl = e.GetComponent<SpotLightComponent>()) {
          if (sl->lightHandle >= 0) { RC::DestroySpotLight(sl->lightHandle); sl->lightHandle = -1; }
      }
      if (auto* al = e.GetComponent<AreaLightComponent>()) {
          if (al->lightHandle >= 0) { RC::DestroyAreaLight(al->lightHandle); al->lightHandle = -1; }
      }
      if (auto* sb = e.GetComponent<SkyboxComponent>()) {
          if (sb->skyboxHandle >= 0) { RC::UnloadSkyBox(sb->skyboxHandle); sb->skyboxHandle = -1; }
      }
      if (auto* sd = e.GetComponent<SkydomeComponent>()) {
          if (sd->skydomeHandle >= 0) { RC::UnloadSkydome(sd->skydomeHandle); sd->skydomeHandle = -1; }
      }
      if (auto* spr = e.GetComponent<SpriteRendererComponent>()) {
          if (spr->spriteHandle >= 0) { RC::UnloadSprite(spr->spriteHandle); spr->spriteHandle = -1; }
      }
      if (auto* water = e.GetComponent<WaterComponent>()) {
          if (water->meshHandle >= 0) { RC::UnloadWater(water->meshHandle); water->meshHandle = -1; }
      }
  }
};
