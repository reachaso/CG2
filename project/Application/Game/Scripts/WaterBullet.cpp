#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ColliderComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/CameraComponent.h"
#include "RenderCommon.h"
#include "Render/Systems/RenderInteractiveWater.h"
#include "Scene.h"
#include <cmath>

/// @brief Water bullet: flies forward, detects collision, spawns splash effect
class WaterBullet : public ScriptableEntity {
public:
    RC::Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    float lifetime = 3.0f;
    float gravity = -3.0f;
    int damage = 1;
    bool initialized = false;
    std::string bulletType = "normal";
    bool isDead = false;

protected:
    void Die() {
        if (isDead) return;
        isDead = true;
        if (Entity* self = GetEntity()) {
            if (auto* tr = self->GetComponent<TransformComponent>()) {
                SpawnSplash(tr->position);
            }
            // エンティティを破棄せず、非アクティブにしてプール（再利用）する
            self->SetActive(false);
            initialized = false;
        }
    }

    void OnCreate() override {
        elapsed_ = 0.0f;
        // ColliderComponentの付与
        Entity* self = GetEntity();
        if (self && !self->HasComponent<ColliderComponent>()) {
            auto* col = &self->AddComponent<ColliderComponent>();
            col->shape = ColliderComponent::Shape::Sphere;
            col->radius = 1.0f;
            col->isTrigger = true; // 物理反射させずイベントだけ取る
        }
    }

    void OnUpdate(float deltaTime) override {
        Entity* self = GetEntity();
        if (self && self->GetTagInt("reused", 0) == 1) {
            elapsed_ = 0.0f;
            markedForDestroy_ = false;
            isDead = false;
            initialized = false;
            velocity = {0.0f, 0.0f, 0.0f};
            self->ClearTag("reused");
        }

        if (markedForDestroy_ || isDead) return;

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        Scene* scene = GetScene();
        if (!scene) return;

        // On first update, determine velocity from entity name + target
        if (!initialized) {
            if (self) {
                std::string name = self->GetName();
                int bType = self->GetTagInt("bullet_type", 0);
                if (bType == 0) bulletType = "normal";
                else if (bType == 1) bulletType = "spread";
                else if (bType == 2) bulletType = "heavy";
                
                float baseSpeed = 40.0f;
                int speedTag = self->GetTagInt("bullet_speed", 0);
                if (speedTag > 0) baseSpeed = static_cast<float>(speedTag) / 10.0f;

                if (bulletType == "heavy") {
                    damage = 3;
                    gravity = -6.0f; // Falls faster due to weight
                    if (speedTag == 0) baseSpeed = 25.0f; // Slower
                } else if (bulletType == "spread") {
                    damage = 1;
                    gravity = -2.5f;
                    if (speedTag == 0) baseSpeed = 35.0f;
                }

                int lifeTag = self->GetTagInt("bullet_lifetime", 0);
                if (lifeTag > 0) this->lifetime = static_cast<float>(lifeTag) / 10.0f;

                if (name == "PlayerBullet" || name == "EnemyBullet") {
                    int dirX = self->GetTagInt("dir_x", -9999);
                    int dirY = self->GetTagInt("dir_y", -9999);
                    int dirZ = self->GetTagInt("dir_z", -9999);
                    
                    if (dirX != -9999 && dirY != -9999 && dirZ != -9999 && bulletType != "spread") {
                        float dx = static_cast<float>(dirX) / 1000.0f;
                        float dy = static_cast<float>(dirY) / 1000.0f;
                        float dz = static_cast<float>(dirZ) / 1000.0f;
                        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                        if (len > 0.01f) { dx /= len; dy /= len; dz /= len; }
                        else { dx = 0.0f; dy = 0.0f; dz = 1.0f; }
                        velocity = { dx * baseSpeed, dy * baseSpeed, dz * baseSpeed };
                    } else if (name == "PlayerBullet" && bulletType == "spread") {
                        float dx = static_cast<float>(self->GetTagInt("dir_x", 0)) / 1000.0f;
                        float dz = static_cast<float>(self->GetTagInt("dir_z", 1000)) / 1000.0f;
                        // Normalize just in case
                        float len = std::sqrt(dx * dx + dz * dz);
                        if (len > 0.01f) { dx /= len; dz /= len; }
                        else { dx = 0.0f; dz = 1.0f; }
                        velocity = { dx * baseSpeed, 1.5f, dz * baseSpeed };
                    } else if (name == "PlayerBullet" && velocity.x == 0.0f && velocity.z == 0.0f) {
                        SetVelocityTowardTarget("Enemy", baseSpeed, "player", tr->position);
                    }
                }
            }
            initialized = true;
        }

        // === Physics ===
        velocity.y += gravity * deltaTime;
        tr->position.x += velocity.x * deltaTime;
        tr->position.y += velocity.y * deltaTime;
        tr->position.z += velocity.z * deltaTime;

        // Lifetime
        elapsed_ += deltaTime;
        if (elapsed_ >= lifetime) {
            Die();
            return;
        }

        // Water surface collision (y < 0)
        if (tr->position.y < 0.0f) {
            Die();
            return;
        }
    }

    void OnCollision(Entity* other) override {
        if (isDead || !other) return;

        Entity* self = GetEntity();
        std::string myName = self ? self->GetName() : "";
        bool isPlayerBullet = (myName == "PlayerBullet");
        std::string targetName = other->GetName();

        // 当たり判定対象の確認 (タグによる疎結合化の準備)
        bool hitEnemy = (isPlayerBullet && (targetName == "Enemy" || other->GetTagInt("is_enemy", 0) == 1));
        bool isCamera = other->HasComponent<CameraComponent>();
        bool hitPlayer = (!isPlayerBullet && isCamera);
        bool hitTerrain = (targetName == "Block" || targetName == "Terrain" || targetName == "Obstacle" || other->GetTagInt("is_terrain", 0) == 1);

        if (hitEnemy || hitPlayer) {
            // ダメージ処理
            int pendingDmg = other->GetTagInt("pending_damage", 0);
            other->SetTag("pending_damage", pendingDmg + damage);

            // プレイヤースコア加算 (カメラをキャッシュしてパフォーマンス向上)
            if (hitEnemy) {
                std::shared_ptr<Entity> cam = cachedCamera_.lock();
                if (!cam || cam->IsPendingDestroy() || !cam->IsActive()) {
                    if (Scene* scene = GetScene()) {
                        for (auto& pe : scene->GetEntities()) {
                            if (pe->HasComponent<CameraComponent>()) {
                                cam = pe;
                                cachedCamera_ = cam;
                                break;
                            }
                        }
                    }
                }
                if (cam) {
                    int scoreAdd = cam->GetTagInt("score_add", 0);
                    cam->SetTag("score_add", scoreAdd + 1);
                }
            }
            Die();
        } else if (hitTerrain) {
            // 地形に当たって消滅
            if (auto* tr = GetComponent<TransformComponent>()) {
                SpawnSplash(tr->position);
            }
            Die();
        }
    }

    private:
    float elapsed_ = 0.0f;
    bool markedForDestroy_ = false;
    std::weak_ptr<Entity> cachedCamera_;

    void SetVelocityTowardTarget(const std::string& targetName, float speed,
                                  const std::string& ownerName, const RC::Vector3& myPos) {
        Scene* scene = GetScene();
        if (!scene) return;

        // If owner specified, use owner's facing direction as fallback
        RC::Vector3 aimDir = { 0.0f, 0.0f, 1.0f };

        // Find target entity
        float nearestDist = 999.0f;
        for (auto& e : scene->GetEntities()) {
            if (e->GetName() == targetName && !e->IsPendingDestroy() && e->IsActive()) {
                auto* eTr = e->GetComponent<TransformComponent>();
                if (eTr) {
                    float dx = eTr->position.x - myPos.x;
                    float dz = eTr->position.z - myPos.z;
                    float d = std::sqrt(dx * dx + dz * dz);
                    if (d < nearestDist && d > 0.01f) {
                        nearestDist = d;
                        aimDir = { dx / d, 0.0f, dz / d };
                    }
                }
            }
        }

        velocity = { aimDir.x * speed, 1.5f, aimDir.z * speed };
    }

    void DestroyBullet() {
        if (markedForDestroy_) return;
        markedForDestroy_ = true;

        Entity* self = GetEntity();
        if (self) {
            self->SetActive(false);
            // Do not delete or unload mesh so we can reuse the entity.
        }
    }

    void SpawnSplash(const RC::Vector3& pos) {
        Scene* scene = GetScene();
        if (!scene) return;

        float waveRadius = (bulletType == "heavy") ? 0.1f : 0.05f;
        float waveStrength = (bulletType == "heavy") ? 1.5f : 0.5f;

        // Strong water surface wave
        RC::WaveSource source;
        source.uv = RC::Vector2((pos.x / 100.0f) + 0.5f, (pos.z / 100.0f) + 0.5f);
        source.radius = waveRadius;
        source.strength = waveStrength;
        RC::AddWaveSource(source);

        if (bulletType == "heavy") {
            // Spawn HeavySplash as a single water column cylinder
            std::shared_ptr<Entity> splash = nullptr;
            for (auto& e : scene->GetEntities()) {
                if (e->GetName() == "HeavySplash" && !e->IsActive() && !e->IsPendingDestroy()) {
                    splash = e;
                    splash->SetActive(true);
                    splash->SetTag("reused", 1);
                    break;
                }
            }
            bool isNew = false;
            if (!splash) {
                splash = scene->CreateEntity("HeavySplash");
                isNew = true;
            }

            splash->SetTag("impact_factor", 150); // Larger impact

            auto* tr = splash->GetComponent<TransformComponent>();
            if (!tr) tr = &splash->AddComponent<TransformComponent>();
            tr->position = pos;
            tr->scale = { 1.5f, 0.1f, 1.5f }; // Start flat and wide

            auto* pm = splash->GetComponent<PrimitiveMeshComponent>();
            if (!pm) {
                pm = &splash->AddComponent<PrimitiveMeshComponent>();
                pm->type = PrimitiveType::Cylinder;
                pm->meshHandle = RC::GenerateCylinder(1.0f, 1.0f);
            } else if (pm->meshHandle < 0) {
                pm->meshHandle = RC::GenerateCylinder(1.0f, 1.0f);
            }

            if (pm->meshHandle >= 0) {
                if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) mat->color = { 0.9f, 0.95f, 1.0f, 1.0f }; 
            }

            auto* nsc = splash->GetComponent<NativeScriptComponent>();
            if (!nsc) {
                nsc = &splash->AddComponent<NativeScriptComponent>();
                nsc->AddScript("HeavySplashParticle");
                nsc->SetScene(scene);
                if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());
            }

            if (isNew) {
                scene->InitDynamicEntityRuntime(*splash);
            }

            if (pm->meshHandle >= 0) {
                if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                    pmTr->scale = tr->scale; pmTr->rotation = tr->rotation; pmTr->translation = tr->position;
                }
            }
        } else {
            // Spawn normal visual splash particles
            const int splashCount = 12;
            for (int i = 0; i < splashCount; ++i) {
                std::shared_ptr<Entity> splash = nullptr;
                for (auto& e : scene->GetEntities()) {
                    if (e->GetName() == "Splash" && !e->IsActive() && !e->IsPendingDestroy()) {
                        splash = e;
                        splash->SetActive(true);
                        splash->SetTag("reused", 1);
                        break;
                    }
                }
                bool isNew = false;
                if (!splash) {
                    splash = scene->CreateEntity("Splash");
                    isNew = true;
                }

                auto* tr = splash->GetComponent<TransformComponent>();
                if (!tr) tr = &splash->AddComponent<TransformComponent>();
                tr->position = pos;
                float s = 0.15f + (i % 4) * 0.05f;
                tr->scale = { s, s, s };

                auto* pm = splash->GetComponent<PrimitiveMeshComponent>();
                if (!pm) {
                    pm = &splash->AddComponent<PrimitiveMeshComponent>();
                    pm->type = PrimitiveType::Sphere;
                    pm->meshHandle = RC::GenerateSphere(1.0f);
                } else if (pm->meshHandle < 0) {
                    pm->meshHandle = RC::GenerateSphere(1.0f);
                }

                if (pm->meshHandle >= 0) {
                    if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                        float r = 0.3f + (i % 3) * 0.15f;
                        float g = 0.6f + (i % 2) * 0.2f;
                        mat->color = { r, g, 1.0f, 0.85f };
                    }
                }

                auto* nsc = splash->GetComponent<NativeScriptComponent>();
                if (!nsc) {
                    nsc = &splash->AddComponent<NativeScriptComponent>();
                    nsc->AddScript("SplashParticle");
                    nsc->SetScene(scene);
                    if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());
                }

                if (isNew) {
                    scene->InitDynamicEntityRuntime(*splash);
                }

                if (pm->meshHandle >= 0) {
                    if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                        pmTr->scale = tr->scale;
                        pmTr->rotation = tr->rotation;
                        pmTr->translation = tr->position;
                    }
                }
            }
        }

        // Spawn Bubbles for all bullet types as an accent
        int bubbleCount = (bulletType == "heavy") ? 8 : 4;
        for (int i = 0; i < bubbleCount; ++i) {
            std::shared_ptr<Entity> bubble = nullptr;
            for (auto& e : scene->GetEntities()) {
                if (e->GetName() == "Bubble" && !e->IsActive() && !e->IsPendingDestroy()) {
                    bubble = e;
                    bubble->SetActive(true);
                    bubble->SetTag("reused", 1);
                    break;
                }
            }
            bool isNew = false;
            if (!bubble) {
                bubble = scene->CreateEntity("Bubble");
                isNew = true;
            }

            auto* tr = bubble->GetComponent<TransformComponent>();
            if (!tr) tr = &bubble->AddComponent<TransformComponent>();
            tr->position = pos;
            float s = 0.05f + (i % 3) * 0.05f;
            tr->scale = { s, s, s };
            
            auto* pm = bubble->GetComponent<PrimitiveMeshComponent>();
            if (!pm) {
                pm = &bubble->AddComponent<PrimitiveMeshComponent>();
                pm->type = PrimitiveType::Sphere;
                pm->meshHandle = RC::GenerateSphere(1.0f);
            } else if (pm->meshHandle < 0) {
                pm->meshHandle = RC::GenerateSphere(1.0f);
            }

            if (pm->meshHandle >= 0) {
                if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) mat->color = { 0.8f, 0.9f, 1.0f, 0.6f };
            }

            auto* nsc = bubble->GetComponent<NativeScriptComponent>();
            if (!nsc) {
                nsc = &bubble->AddComponent<NativeScriptComponent>();
                nsc->AddScript("BubbleParticle");
                nsc->SetScene(scene);
                if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());
            }

            if (isNew) {
                scene->InitDynamicEntityRuntime(*bubble);
            }

            if (pm->meshHandle >= 0) {
                if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                    pmTr->scale = tr->scale; pmTr->rotation = tr->rotation; pmTr->translation = tr->position;
                }
            }
        }
    }
};

REGISTER_SCRIPT(WaterBullet)
