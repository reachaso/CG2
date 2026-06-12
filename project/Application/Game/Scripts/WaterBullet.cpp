#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ColliderComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
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

protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
    }

    void OnUpdate(float deltaTime) override {
        if (markedForDestroy_) return;

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        Scene* scene = GetScene();
        if (!scene) return;

        // On first update, determine velocity from entity name + target
        if (!initialized) {
            Entity* self = GetEntity();
            if (self) {
                std::string name = self->GetName();
                if (name == "PlayerBullet") {
                    SetVelocityTowardTarget("Enemy", 40.0f, "player", tr->position);
                } else if (name == "EnemyBullet") {
                    SetVelocityTowardTarget("player", 25.0f, "", tr->position);
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
            DestroyBullet();
            return;
        }

        // Water surface collision (y < 0)
        if (tr->position.y < 0.0f) {
            SpawnSplash(tr->position);
            DestroyBullet();
            return;
        }

        // === Collision detection ===
        Entity* self = GetEntity();
        std::string myName = self ? self->GetName() : "";
        bool isPlayerBullet = (myName == "PlayerBullet");
        bool didHit = false;
        RC::Vector3 hitPosition = {};

        for (auto& e : scene->GetEntities()) {
            if (e.get() == self) continue;
            if (e->IsPendingDestroy()) continue;

            std::string targetName = e->GetName();

            // PlayerBullet hits Enemy, EnemyBullet hits player
            bool shouldHit = (isPlayerBullet && targetName == "Enemy") ||
                             (!isPlayerBullet && targetName == "player");
            if (!shouldHit) continue;

            auto* eTr = e->GetComponent<TransformComponent>();
            if (!eTr) continue;

            float dx = eTr->position.x - tr->position.x;
            float dy = eTr->position.y - tr->position.y;
            float dz = eTr->position.z - tr->position.z;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            float hitRadius = 1.5f; // Generous hit radius

            if (dist < hitRadius) {
                // Use tag system to communicate damage
                int pendingDmg = e->GetTagInt("pending_damage", 0);
                e->SetTag("pending_damage", pendingDmg + damage);

                // If it's a player bullet hitting enemy, also tell player to add score
                if (isPlayerBullet) {
                    for (auto& pe : scene->GetEntities()) {
                        if (pe->GetName() == "player") {
                            int scoreAdd = pe->GetTagInt("score_add", 0);
                            pe->SetTag("score_add", scoreAdd + 1);
                            break;
                        }
                    }
                }

                hitPosition = tr->position;
                didHit = true;
                break; // Exit loop BEFORE modifying entities
            }
        }

        // Spawn splash and destroy AFTER loop to avoid iterator invalidation
        if (didHit) {
            SpawnSplash(hitPosition);
            DestroyBullet();
        }
    }

private:
    float elapsed_ = 0.0f;
    bool markedForDestroy_ = false;

    void SetVelocityTowardTarget(const std::string& targetName, float speed,
                                  const std::string& ownerName, const RC::Vector3& myPos) {
        Scene* scene = GetScene();
        if (!scene) return;

        // If owner specified, use owner's facing direction as fallback
        RC::Vector3 aimDir = { 0.0f, 0.0f, 1.0f };

        // Find target entity
        float nearestDist = 999.0f;
        for (auto& e : scene->GetEntities()) {
            if (e->GetName() == targetName && !e->IsPendingDestroy()) {
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
        Scene* scene = GetScene();
        if (self && scene) {
            auto* pm = self->GetComponent<PrimitiveMeshComponent>();
            if (pm && pm->meshHandle >= 0) {
                RC::UnloadPrimitiveMesh(pm->meshHandle);
                pm->meshHandle = -1;
            }
            scene->RemoveEntity(self->GetId());
        }
    }

    void SpawnSplash(const RC::Vector3& pos) {
        Scene* scene = GetScene();
        if (!scene) return;

        // Strong water surface wave
        RC::WaveSource source;
        source.uv = RC::Vector2((pos.x / 100.0f) + 0.5f, (pos.z / 100.0f) + 0.5f);
        source.radius = 0.05f;
        source.strength = 0.5f;
        RC::AddWaveSource(source);

        // Spawn visual splash particles
        const int splashCount = 8;
        for (int i = 0; i < splashCount; ++i) {
            auto splash = scene->CreateEntity("Splash");

            auto& tr = splash->AddComponent<TransformComponent>();
            tr.position = pos;
            float s = 0.06f + (i % 3) * 0.03f;
            tr.scale = { s, s, s };

            auto& pm = splash->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Sphere;
            pm.meshHandle = RC::GenerateSphere(s);

            if (pm.meshHandle >= 0) {
                if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm.meshHandle)) {
                    float r = 0.3f + (i % 3) * 0.15f;
                    float g = 0.6f + (i % 2) * 0.2f;
                    mat->color = { r, g, 1.0f, 0.85f };
                }
            }

            auto& nsc = splash->AddComponent<NativeScriptComponent>();
            nsc.Bind("SplashParticle");
            nsc.SetScene(scene);
            if (GetSceneContext()) nsc.SetSceneContext(GetSceneContext());

            scene->InitDynamicEntityRuntime(*splash);
        }
    }
};

REGISTER_SCRIPT(WaterBullet)
