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
        RC::Vector3 oldPos = tr->position;
        velocity.y += gravity * deltaTime;
        tr->position.x += velocity.x * deltaTime;
        tr->position.y += velocity.y * deltaTime;
        tr->position.z += velocity.z * deltaTime;

        // Continuous Collision Detection (CCD) Raycast
        RC::Vector3 diff = { tr->position.x - oldPos.x, tr->position.y - oldPos.y, tr->position.z - oldPos.z };
        float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (distSq > 0.001f) {
            float dist = std::sqrt(distSq);
            RC::Ray ray;
            ray.origin = oldPos;
            ray.direction = { diff.x / dist, diff.y / dist, diff.z / dist };
            
            float closestDist = dist;
            Entity* hitEntity = nullptr;
            RC::Vector3 hitPoint = tr->position;
            
            bool isPlayerBullet = (self->GetName() == "PlayerBullet");

            for (auto& e : scene->GetEntities()) {
                if (e.get() == self || !e->IsActive() || e->IsPendingDestroy()) continue;
                
                const std::string& eName = e->GetName();
                // 弾やパーティクルなど、確実に当たり判定対象外のものはGetComponentやGetTagの前に除外する（超高速化）
                if (eName == "Splash" || eName == "Bubble" || eName == "HeavySplash" || 
                    eName == "PlayerBullet" || eName == "EnemyBullet" || eName == "Wake" ||
                    eName == "Effects" || eName == "PlayerBullets" || eName == "EnemyBullets") {
                    continue;
                }

                auto* eCol = e->GetComponent<ColliderComponent>();
                auto* eTr = e->GetComponent<TransformComponent>();
                if (!eCol || !eCol->IsEnabled() || !eTr) continue;

                // 疎結合: タグで当たり判定対象かチェック
                bool isTarget = false;
                if (isPlayerBullet) {
                    if (e->GetTagInt("is_enemy", 0) == 1 || e->GetName() == "Block" || e->GetName() == "Terrain") isTarget = true;
                } else {
                    if (e->GetTagInt("is_player", 0) == 1 || e->GetName() == "Block" || e->GetName() == "Terrain") isTarget = true;
                }
                if (!isTarget) continue;

                RC::Vector3 scaledCenter = {
                    eCol->center.x * eTr->scale.x,
                    eCol->center.y * eTr->scale.y,
                    eCol->center.z * eTr->scale.z
                };
                RC::Vector3 center = RC::Add(eTr->position, scaledCenter);
                
                float t = -1.0f;
                bool hit = false;
                if (eCol->shape == ColliderComponent::Shape::Sphere) {
                    float r = eCol->radius * (std::max)((std::max)(std::abs(eTr->scale.x), std::abs(eTr->scale.y)), std::abs(eTr->scale.z));
                    auto* myCol = GetComponent<ColliderComponent>();
                    if (myCol) {
                        float myR = myCol->radius * (std::max)((std::max)(std::abs(tr->scale.x), std::abs(tr->scale.y)), std::abs(tr->scale.z));
                        r += myR;
                    }
                    hit = RC::IntersectRaySphere(ray, center, r, t);
                } else if (eCol->shape == ColliderComponent::Shape::AABB) {
                    RC::Vector3 h = { std::abs(eCol->size.x * eTr->scale.x * 0.5f), std::abs(eCol->size.y * eTr->scale.y * 0.5f), std::abs(eCol->size.z * eTr->scale.z * 0.5f) };
                    auto* myCol = GetComponent<ColliderComponent>();
                    if (myCol) {
                        float myR = myCol->radius * (std::max)((std::max)(std::abs(tr->scale.x), std::abs(tr->scale.y)), std::abs(tr->scale.z));
                        h.x += myR; h.y += myR; h.z += myR;
                    }
                    RC::Vector3 minBox = RC::Sub(center, h);
                    RC::Vector3 maxBox = RC::Add(center, h);
                    hit = RC::IntersectRayAABB(ray, minBox, maxBox, t);
                }

                if (hit && t >= 0.0f && t <= closestDist) {
                    closestDist = t;
                    hitEntity = e.get();
                    hitPoint = RC::Add(ray.origin, RC::Mul(ray.direction, t));
                }
            }

            if (hitEntity) {
                tr->position = hitPoint;
                OnCollision(hitEntity, hitPoint);
                if (isDead) return;
            }
        }

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

    void OnCollision(Entity* other, const RC::Vector3& contactPoint = {}) override {
        if (isDead || !other) return;

        Entity* self = GetEntity();
        std::string myName = self ? self->GetName() : "";
        bool isPlayerBullet = (myName == "PlayerBullet");
        std::string targetName = other->GetName();

        // 当たった瞬間をログで確認
        printf("[Collision] %s hit %s at (%.2f, %.2f, %.2f)\n", 
               myName.c_str(), targetName.c_str(), 
               contactPoint.x, contactPoint.y, contactPoint.z);

        // 当たり判定対象の確認 (タグによる疎結合化)
        bool hitEnemy = (isPlayerBullet && other->GetTagInt("is_enemy", 0) == 1);
        bool hitPlayer = (!isPlayerBullet && other->GetTagInt("is_player", 0) == 1);
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

    uint64_t effectsFolderGuid_ = 0;

    uint64_t GetEffectsFolder(Scene* scene) {
        if (effectsFolderGuid_ != 0) return effectsFolderGuid_;
        for (auto& e : scene->GetEntities()) {
            if (e->GetName() == "Effects" && e->IsFolder()) {
                effectsFolderGuid_ = e->Guid();
                return effectsFolderGuid_;
            }
        }
        auto folder = scene->CreateEntity("Effects");
        folder->SetIsFolder(true);
        effectsFolderGuid_ = folder->Guid();
        return effectsFolderGuid_;
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

        int splashCount = (bulletType == "heavy") ? 0 : 12;
        int bubbleCount = (bulletType == "heavy") ? 8 : 4;
        
        std::vector<std::shared_ptr<Entity>> inactiveSplashes;
        std::vector<std::shared_ptr<Entity>> inactiveBubbles;
        std::shared_ptr<Entity> inactiveHeavySplash = nullptr;

        for (auto& e : scene->GetEntities()) {
            if (!e->IsActive() && !e->IsPendingDestroy()) {
                const std::string& name = e->GetName();
                if (splashCount > 0 && name == "Splash" && inactiveSplashes.size() < splashCount) {
                    inactiveSplashes.push_back(e);
                } else if (bubbleCount > 0 && name == "Bubble" && inactiveBubbles.size() < bubbleCount) {
                    inactiveBubbles.push_back(e);
                } else if (bulletType == "heavy" && !inactiveHeavySplash && name == "HeavySplash") {
                    inactiveHeavySplash = e;
                }
            }
            // 必要な数が集まったら即座にループを抜ける（O(N)ループ回避）
            if (inactiveSplashes.size() >= splashCount && 
                inactiveBubbles.size() >= bubbleCount && 
                (bulletType != "heavy" || inactiveHeavySplash)) {
                break;
            }
        }

        if (bulletType == "heavy") {
            // Spawn HeavySplash as a single water column cylinder
            std::shared_ptr<Entity> splash = inactiveHeavySplash;
            bool isNew = false;
            if (splash) {
                splash->SetActive(true);
                splash->SetTag("reused", 1);
            } else {
                splash = scene->CreateEntity("HeavySplash");
                isNew = true;
            }

            splash->SetTag("impact_factor", 150); // Larger impact

            auto* tr = splash->GetComponent<TransformComponent>();
            if (!tr) tr = &splash->AddComponent<TransformComponent>();
            tr->position = pos;
            splash->SetParentGuid(GetEffectsFolder(scene));
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
            for (int i = 0; i < splashCount; ++i) {
                std::shared_ptr<Entity> splash = nullptr;
                bool isNew = false;
                if (i < inactiveSplashes.size()) {
                    splash = inactiveSplashes[i];
                    splash->SetActive(true);
                    splash->SetTag("reused", 1);
                } else {
                    splash = scene->CreateEntity("Splash");
                    isNew = true;
                }

                auto* tr = splash->GetComponent<TransformComponent>();
                if (!tr) tr = &splash->AddComponent<TransformComponent>();
                tr->position = pos;
                splash->SetParentGuid(GetEffectsFolder(scene));
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
        for (int i = 0; i < bubbleCount; ++i) {
            std::shared_ptr<Entity> bubble = nullptr;
            bool isNew = false;
            if (i < inactiveBubbles.size()) {
                bubble = inactiveBubbles[i];
                bubble->SetActive(true);
                bubble->SetTag("reused", 1);
            } else {
                bubble = scene->CreateEntity("Bubble");
                isNew = true;
            }

            auto* tr = bubble->GetComponent<TransformComponent>();
            if (!tr) tr = &bubble->AddComponent<TransformComponent>();
            tr->position = pos;
            bubble->SetParentGuid(GetEffectsFolder(scene));
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
