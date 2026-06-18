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
#include <iostream>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief Enemy AI: chases player, shoots water bullets, takes damage
class EnemyAI : public ScriptableEntity {
public:
    float moveSpeed = 3.0f;
    float shootInterval = 1.5f;
    float bulletSpeed = 10.0f;
    int hp = 10;
    int maxHp = 10;
    float preferredDist = 15.0f;  ///< Desired distance to player
    float weight = 5.0f;          ///< Weight for splash calculation
    bool isDead = false;
    float deathTimer = 0.0f;

protected:
    void OnCreate() override {
        std::cout << "[EnemyAI] OnCreate\n";
        shootTimer_ = shootInterval * 0.5f; // Start with half delay
        strafeAngle_ = 0.0f;
    }

    void OnUpdate(float deltaTime) override {
        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        float prevY = tr->position.y;

        if (isDead) {
            deathTimer += deltaTime;
            verticalVel_ -= 9.8f * deltaTime; // 沈む時の重力（水中の抵抗などを考慮しても良いが一旦通常重力）
            tr->position.y += verticalVel_ * deltaTime;
            // 空中から水面に落ちた時だけ水しぶきを上げる
            if (prevY > 0.1f && tr->position.y <= 0.1f) {
                float impactSpeed = std::abs(verticalVel_);
                SpawnSplash({tr->position.x, 0.0f, tr->position.z}, impactSpeed, weight);
            }
            return;
        }

        // 生きている時も空中にいれば落下する
        if (tr->position.y > 0.0f) {
            verticalVel_ -= 9.8f * deltaTime;
            tr->position.y += verticalVel_ * deltaTime;

            // 水面に着水
            if (tr->position.y <= 0.0f) {
                tr->position.y = 0.0f;
                if (prevY > 0.1f) {
                    float impactSpeed = std::abs(verticalVel_);
                    SpawnSplash({tr->position.x, 0.0f, tr->position.z}, impactSpeed, weight);
                }
                verticalVel_ = 0.0f;
            }
        } else {
            tr->position.y = 0.0f;
            verticalVel_ = 0.0f;
        }

        // Process pending damage from tag system
        Entity* self = GetEntity();
        if (self) {
            int dmg = self->GetTagInt("pending_damage", 0);
            if (dmg > 0) {
                self->ClearTag("pending_damage");
                TakeDamage(dmg);
            }
        }

        Scene* scene = GetScene();
        if (!scene) return;

        // Find player
        Entity* player = nullptr;
        for (auto& e : scene->GetEntities()) {
            if (e->GetName() == "player") {
                player = e.get();
                break;
            }
        }
        if (!player) return;

        auto* playerTr = player->GetComponent<TransformComponent>();
        if (!playerTr) return;

        RC::Vector3 toPlayer = {
            playerTr->position.x - tr->position.x,
            0.0f,
            playerTr->position.z - tr->position.z
        };
        float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

        if (dist > 0.01f) {
            RC::Vector3 dirToPlayer = { toPlayer.x / dist, 0.0f, toPlayer.z / dist };

            // Strafe movement (orbit around player)
            strafeAngle_ += deltaTime * 0.8f;
            float strafeX = -dirToPlayer.z * std::sin(strafeAngle_) * 0.5f;
            float strafeZ =  dirToPlayer.x * std::sin(strafeAngle_) * 0.5f;

            // Move toward or away from preferred distance
            float approach = 0.0f;
            if (dist > preferredDist + 3.0f) approach = 1.0f;
            else if (dist < preferredDist - 3.0f) approach = -1.0f;

            float moveX = (dirToPlayer.x * approach + strafeX) * moveSpeed * deltaTime;
            float moveZ = (dirToPlayer.z * approach + strafeZ) * moveSpeed * deltaTime;

            tr->position.x += moveX;
            tr->position.z += moveZ;

            // Clamp to play area
            const float areaLimit = 45.0f;
            if (tr->position.x > areaLimit)  tr->position.x = areaLimit;
            if (tr->position.x < -areaLimit) tr->position.x = -areaLimit;
            if (tr->position.z > areaLimit)  tr->position.z = areaLimit;
            if (tr->position.z < -areaLimit) tr->position.z = -areaLimit;

            // Water wave interaction
            if (std::abs(moveX) > 0.001f || std::abs(moveZ) > 0.001f) {
                float velocity = std::sqrt(moveX * moveX + moveZ * moveZ) / deltaTime;

                if (!firstUpdate_) {
                    RC::WaveSource sourcePrev;
                    sourcePrev.uv = RC::Vector2((prevPosition_.x / 100.0f) + 0.5f, (prevPosition_.z / 100.0f) + 0.5f);
                    sourcePrev.radius = 0.035f;
                    sourcePrev.strength = -velocity * 0.02f;
                    RC::AddWaveSource(sourcePrev);
                }

                RC::WaveSource source;
                source.uv = RC::Vector2((tr->position.x / 100.0f) + 0.5f, (tr->position.z / 100.0f) + 0.5f);
                source.radius = 0.035f;
                source.strength = velocity * 0.02f;
                RC::AddWaveSource(source);

            }
            prevPosition_ = tr->position;
            firstUpdate_ = false;

            // Face toward player
            tr->rotation.y = std::atan2(dirToPlayer.x, dirToPlayer.z);
        }

        // === Shooting ===
        shootTimer_ -= deltaTime;
        if (shootTimer_ <= 0.0f && dist < 40.0f) {
            ShootAt(tr->position, playerTr->position);
            shootTimer_ = shootInterval + (std::sin(strafeAngle_ * 3.0f) * 0.5f);
        }
    }

    void OnRender() override {
        DrawEnemyHPBar();
    }

    void OnDestroy() override {
        std::cout << "[EnemyAI] OnDestroy\n";
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::DragFloat("Move Speed##E", &moveSpeed, 0.1f, 0.1f, 20.0f);
        ImGui::DragFloat("Shoot Interval##E", &shootInterval, 0.05f, 0.1f, 5.0f);
        ImGui::DragFloat("Bullet Speed##E", &bulletSpeed, 0.1f, 1.0f, 50.0f);
        ImGui::DragInt("HP##E", &hp, 1, 0, maxHp);
        ImGui::DragFloat("Preferred Dist##E", &preferredDist, 0.5f, 5.0f, 40.0f);
        ImGui::DragFloat("Weight##E", &weight, 0.1f, 0.1f, 50.0f);
#endif
    }

    /// @brief Take damage from a bullet
    void TakeDamage(int damage) {
        if (isDead) return;
        hp -= damage;
        if (hp <= 0) {
            hp = 0;
            isDead = true;
            deathTimer = 0.0f;
            // タグでシーン側に撃破を通知
            if (Entity* self = GetEntity()) {
                self->SetTag("enemy_defeated", 1);
            }
        }
    }

private:
    float shootTimer_ = 0.0f;
    float strafeAngle_ = 0.0f;
    RC::Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    bool firstUpdate_ = true;
    float verticalVel_ = 0.0f;

    void ShootAt(const RC::Vector3& origin, const RC::Vector3& target) {
        Scene* scene = GetScene();
        if (!scene) return;

        // Direction to player
        RC::Vector3 dir = {
            target.x - origin.x,
            0.0f,
            target.z - origin.z
        };
        float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        if (len < 0.01f) return;
        dir.x /= len;
        dir.z /= len;

        // Create bullet entity
        auto bullet = scene->CreateEntity("EnemyBullet");

        auto& tr = bullet->AddComponent<TransformComponent>();
        tr.position = { origin.x + dir.x * 1.5f, origin.y + 0.5f, origin.z + dir.z * 1.5f };
        tr.scale = { 0.25f, 0.25f, 0.25f };

        auto& pm = bullet->AddComponent<PrimitiveMeshComponent>();
        pm.type = PrimitiveType::Sphere;
        pm.meshHandle = RC::GenerateSphere(0.25f);

        auto& col = bullet->AddComponent<ColliderComponent>();
        col.shape = ColliderComponent::Shape::Sphere;
        col.radius = 1.0f;
        col.isTrigger = true;

        auto& nsc = bullet->AddComponent<NativeScriptComponent>();
        nsc.Bind("WaterBullet");
        nsc.SetScene(scene);
        if (GetSceneContext()) nsc.SetSceneContext(GetSceneContext());

        // Set color (reddish water)
        if (pm.meshHandle >= 0) {
            if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm.meshHandle)) {
                mat->color = { 1.0f, 0.3f, 0.3f, 0.85f };
            }
        }

        scene->InitDynamicEntityRuntime(*bullet);

        // 即座に PrimitiveMesh の Transform を同期して原点でのチラつきを防ぐ
        if (pm.meshHandle >= 0) {
            if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm.meshHandle)) {
                pmTr->scale = tr.scale;
                pmTr->rotation = tr.rotation;
                pmTr->translation = tr.position;
            }
        }
    }

    void DrawEnemyHPBar() {
        SceneContext* ctx = GetSceneContext();
        if (!ctx) return;

        float screenW = static_cast<float>(ctx->app->width);
        // Draw HP bar at top-right
        float barX = screenW - 230.0f;
        float barY = 60.0f;
        float barW = 200.0f;
        float barH = 16.0f;

        // Background
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH },
                    { 0.3f, 0.05f, 0.05f, 0.8f });

        // Foreground
        float hpRatio = static_cast<float>(hp) / static_cast<float>(maxHp);
        RC::Vector4 hpColor = { 0.9f, 0.2f, 0.2f, 0.9f };
        RC::DrawBox({ barX, barY }, { barX + barW * hpRatio, barY + barH }, hpColor);

        // Border
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH },
                    { 1.0f, 1.0f, 1.0f, 0.5f }, kWire);
    }

    void SpawnSplash(const RC::Vector3& pos, float impactSpeed, float weightVal) {
        Scene* scene = GetScene();
        if (!scene) return;

        // 計算式のベース
        // impactSpeed は大体 1.0 ~ 20.0 などを想定
        // weightVal は 1.0 ~ 10.0 などを想定
        float impactFactor = (impactSpeed * 0.1f) * (weightVal * 0.2f);
        if (impactFactor < 0.1f) impactFactor = 0.1f;
        if (impactFactor > 5.0f) impactFactor = 5.0f;

        // 水面の波紋を強く発生させる
        RC::WaveSource source;
        source.uv = RC::Vector2((pos.x / 100.0f) + 0.5f, (pos.z / 100.0f) + 0.5f);
        source.radius = 0.05f + (impactFactor * 0.02f);
        source.strength = 0.5f + (impactFactor * 0.5f);
        RC::AddWaveSource(source);

        // 水しぶきパーティクルを複数生成
        int splashCount = static_cast<int>(8 + (impactFactor * 8));
        if (splashCount > 30) splashCount = 30; // Max

        for (int i = 0; i < splashCount; ++i) {
            auto splash = scene->CreateEntity("Splash");

            // タグでパーティクルの勢い（スケール）を渡す
            splash->SetTag("impact_factor", static_cast<int>(impactFactor * 100));

            auto& tr = splash->AddComponent<TransformComponent>();
            tr.position = pos;
            float s = (0.15f + (i % 4) * 0.05f) * (1.0f + impactFactor * 0.3f);
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

            // 即座に PrimitiveMesh の Transform を同期して原点でのチラつきを防ぐ
            if (pm.meshHandle >= 0) {
                if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm.meshHandle)) {
                    pmTr->scale = tr.scale;
                    pmTr->rotation = tr.rotation;
                    pmTr->translation = tr.position;
                }
            }
        }
    }
};

REGISTER_SCRIPT(EnemyAI)
