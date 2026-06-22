#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ColliderComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "Input/Input.h"
#include "Common/EngineConfig.h"
#include "RenderCommon.h"
#include "Render/Systems/RenderInteractiveWater.h"
#include "Scene.h"
#include <iostream>
#include <cmath>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief Player controller combining movement and shooting
class PlayerController : public ScriptableEntity {
public:
    float speed = 5.0f;
    int hp = 5;
    int maxHp = 5;
    float shootCooldown = 0.3f;
    float bulletSpeed = 15.0f;
    float invincibleTimer = 0.0f;
    float invincibleDuration = 1.0f;
    int score = 0;
    float weight = 5.0f;
    bool isDead = false;

    enum class WeaponType { Normal, Spread, Heavy };
    WeaponType currentWeapon = WeaponType::Normal;

protected:
    void OnCreate() override {
        std::cout << "[PlayerController] OnCreate\n";
        shootTimer_ = 0.0f;
    }

    void OnUpdate(float deltaTime) override {
        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        float prevY = tr->position.y;

        if (isDead) {
            verticalVel_ -= 9.8f * deltaTime;
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
            int scoreAdd = self->GetTagInt("score_add", 0);
            if (scoreAdd > 0) {
                self->ClearTag("score_add");
                score += scoreAdd;
            }
        }

        Input* input = Input::GetInstance();
        if (!input) return;

        // === Invincibility timer ===
        if (invincibleTimer > 0.0f) {
            invincibleTimer -= deltaTime;
        }

        // === Weapon Switching ===
        if (input->IsKeyPressed(DIK_1)) currentWeapon = WeaponType::Normal;
        if (input->IsKeyPressed(DIK_2)) currentWeapon = WeaponType::Spread;
        if (input->IsKeyPressed(DIK_3)) currentWeapon = WeaponType::Heavy;

        if (input->IsXInputConnected()) {
            bool lb = input->IsXInputButtonPressed(XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool rb = input->IsXInputButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
            if (lb && !prevLB_) {
                int w = static_cast<int>(currentWeapon) - 1;
                if (w < 0) w = 2;
                currentWeapon = static_cast<WeaponType>(w);
            }
            if (rb && !prevRB_) {
                int w = static_cast<int>(currentWeapon) + 1;
                if (w > 2) w = 0;
                currentWeapon = static_cast<WeaponType>(w);
            }
            prevLB_ = lb;
            prevRB_ = rb;
        }

        // === Shoot cooldown ===
        if (shootTimer_ > 0.0f) {
            shootTimer_ -= deltaTime;
        }

        // === Movement ===
        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (input->IsKeyPressed(DIK_W) || input->IsKeyPressed(DIK_UP))    moveZ += 1.0f;
        if (input->IsKeyPressed(DIK_S) || input->IsKeyPressed(DIK_DOWN))  moveZ -= 1.0f;
        if (input->IsKeyPressed(DIK_D) || input->IsKeyPressed(DIK_RIGHT)) moveX += 1.0f;
        if (input->IsKeyPressed(DIK_A) || input->IsKeyPressed(DIK_LEFT))  moveX -= 1.0f;

        if (input->IsXInputConnected()) {
            SHORT thumbX = input->GetXInputThumbLX();
            SHORT thumbY = input->GetXInputThumbLY();
            const float deadZone = 8000.0f;
            if (abs(thumbX) > deadZone) moveX += static_cast<float>(thumbX) / 32767.0f;
            if (abs(thumbY) > deadZone) moveZ += static_cast<float>(thumbY) / 32767.0f;

            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_UP))    moveZ += 1.0f;
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN))  moveZ -= 1.0f;
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_RIGHT)) moveX += 1.0f;
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_LEFT))  moveX -= 1.0f;
        }

        if (moveX >  1.0f) moveX =  1.0f;
        if (moveX < -1.0f) moveX = -1.0f;
        if (moveZ >  1.0f) moveZ =  1.0f;
        if (moveZ < -1.0f) moveZ = -1.0f;

        tr->position.x += moveX * speed * deltaTime;
        tr->position.z += moveZ * speed * deltaTime;

        // Clamp to play area
        const float areaLimit = 45.0f;
        if (tr->position.x > areaLimit)  tr->position.x = areaLimit;
        if (tr->position.x < -areaLimit) tr->position.x = -areaLimit;
        if (tr->position.z > areaLimit)  tr->position.z = areaLimit;
        if (tr->position.z < -areaLimit) tr->position.z = -areaLimit;

        // Water wave interaction
        if (moveX != 0.0f || moveZ != 0.0f) {
            float velocity = std::sqrt(moveX * moveX + moveZ * moveZ) * speed;

            if (!firstUpdate_) {
                RC::WaveSource sourcePrev;
                sourcePrev.uv = RC::Vector2((prevPosition_.x / 100.0f) + 0.5f, (prevPosition_.z / 100.0f) + 0.5f);
                sourcePrev.radius = 0.03f;
                sourcePrev.strength = -velocity * 0.03f;
                RC::AddWaveSource(sourcePrev);
            }

            RC::WaveSource source;
            source.uv = RC::Vector2((tr->position.x / 100.0f) + 0.5f, (tr->position.z / 100.0f) + 0.5f);
            source.radius = 0.03f;
            source.strength = velocity * 0.03f;
            RC::AddWaveSource(source);

        }
        
        prevPosition_ = tr->position;
        firstUpdate_ = false;

        // Store facing direction for shooting
        if (moveX != 0.0f || moveZ != 0.0f) {
            float len = std::sqrt(moveX * moveX + moveZ * moveZ);
            facingDir_ = { moveX / len, 0.0f, moveZ / len };
        }

        // === Shooting ===
        bool wantShoot = input->IsKeyPressed(DIK_SPACE);
        if (input->IsXInputConnected()) {
            wantShoot = wantShoot || input->IsXInputButtonPressed(XINPUT_GAMEPAD_A);
        }

        if (wantShoot && shootTimer_ <= 0.0f) {
            Shoot(tr->position);
            if (currentWeapon == WeaponType::Heavy) shootTimer_ = shootCooldown * 2.0f;
            else shootTimer_ = shootCooldown;
        }
    }

    void OnRender() override {
        DrawHUD();
    }

    void OnDestroy() override {
        std::cout << "[PlayerController] OnDestroy\n";
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::DragFloat("Speed##PC", &speed, 0.1f, 0.1f, 50.0f);
        ImGui::DragFloat("Shoot Cooldown##PC", &shootCooldown, 0.01f, 0.05f, 2.0f);
        ImGui::DragFloat("Bullet Speed##PC", &bulletSpeed, 0.1f, 1.0f, 50.0f);
        ImGui::DragInt("HP##PC", &hp, 1, 0, maxHp);
        ImGui::DragFloat("Weight##PC", &weight, 0.1f, 0.1f, 50.0f);
        ImGui::Text("Score: %d", score);
#endif
    }

    /// @brief Take damage from a bullet
    void TakeDamage(int damage) {
        if (invincibleTimer > 0.0f || isDead) return;
        hp -= damage;
        invincibleTimer = invincibleDuration;
        if (hp <= 0) {
            hp = 0;
            isDead = true;
            // タグでシーン側に死亡を通知
            if (Entity* self = GetEntity()) {
                self->SetTag("game_over", 1);
            }
        }
    }

    RC::Vector3 GetFacingDir() const { return facingDir_; }

private:
    float shootTimer_ = 0.0f;
    RC::Vector3 facingDir_ = { 0.0f, 0.0f, 1.0f };
    RC::Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    bool firstUpdate_ = true;
    float verticalVel_ = 0.0f;
    bool prevLB_ = false;
    bool prevRB_ = false;

    void Shoot(const RC::Vector3& origin) {
        Scene* scene = GetScene();
        if (!scene) return;

        int numBullets = 1;
        float spreadAngle = 0.0f;
        if (currentWeapon == WeaponType::Spread) {
            numBullets = 3;
            spreadAngle = 15.0f * 3.14159f / 180.0f; // 15 degrees
        }

        for (int i = 0; i < numBullets; ++i) {
            auto bullet = scene->CreateEntity("PlayerBullet");

            auto& tr = bullet->AddComponent<TransformComponent>();
            tr.position = { origin.x, origin.y + 0.5f, origin.z };

            float scale = 0.3f;
            if (currentWeapon == WeaponType::Heavy) scale = 0.6f;
            else if (currentWeapon == WeaponType::Spread) scale = 0.2f;
            tr.scale = { scale, scale, scale };

            if (currentWeapon == WeaponType::Normal) bullet->SetTag("bullet_type", 0);
            else if (currentWeapon == WeaponType::Spread) bullet->SetTag("bullet_type", 1);
            else if (currentWeapon == WeaponType::Heavy) bullet->SetTag("bullet_type", 2);

            if (currentWeapon == WeaponType::Spread) {
                float angleOff = (i - 1) * spreadAngle;
                float c = std::cos(angleOff);
                float s = std::sin(angleOff);
                float dx = facingDir_.x * c - facingDir_.z * s;
                float dz = facingDir_.x * s + facingDir_.z * c;
                bullet->SetTag("dir_x", static_cast<int>(dx * 1000));
                bullet->SetTag("dir_z", static_cast<int>(dz * 1000));
            }

            auto& pm = bullet->AddComponent<PrimitiveMeshComponent>();
            pm.type = PrimitiveType::Sphere;
            pm.meshHandle = RC::GenerateSphere(scale);

            auto& col = bullet->AddComponent<ColliderComponent>();
            col.shape = ColliderComponent::Shape::Sphere;
            col.radius = (currentWeapon == WeaponType::Heavy) ? 1.5f : 1.0f;
            col.isTrigger = true;

            auto& nsc = bullet->AddComponent<NativeScriptComponent>();
            nsc.Bind("WaterBullet");
            nsc.SetScene(scene);
            if (GetSceneContext()) nsc.SetSceneContext(GetSceneContext());

            if (pm.meshHandle >= 0) {
                if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm.meshHandle)) {
                    if (currentWeapon == WeaponType::Normal) mat->color = { 0.2f, 0.6f, 1.0f, 0.85f };
                    else if (currentWeapon == WeaponType::Spread) mat->color = { 0.2f, 0.8f, 0.4f, 0.85f };
                    else if (currentWeapon == WeaponType::Heavy) mat->color = { 0.8f, 0.3f, 0.1f, 0.85f };
                }
            }

            scene->InitDynamicEntityRuntime(*bullet);

            if (pm.meshHandle >= 0) {
                if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm.meshHandle)) {
                    pmTr->scale = tr.scale;
                    pmTr->rotation = tr.rotation;
                    pmTr->translation = tr.position;
                }
            }
        }
    }

    void DrawHUD() {
        SceneContext* ctx = GetSceneContext();
        if (!ctx) return;

        float screenW = static_cast<float>(ctx->app->width);
        float screenH = static_cast<float>(ctx->app->height);

        // === Player HP Bar ===
        float barX = 20.0f;
        float barY = screenH - 50.0f;
        float barW = 200.0f;
        float barH = 20.0f;

        // Background (dark red)
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH },
                    { 0.3f, 0.05f, 0.05f, 0.8f });

        // Foreground (green->red based on HP)
        float hpRatio = static_cast<float>(hp) / static_cast<float>(maxHp);
        RC::Vector4 hpColor = { 1.0f - hpRatio, hpRatio, 0.1f, 0.9f };
        RC::DrawBox({ barX, barY }, { barX + barW * hpRatio, barY + barH }, hpColor);

        // Border
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH },
                    { 1.0f, 1.0f, 1.0f, 0.5f }, kWire);

        // === Weapon Indicator ===
        float wX = 20.0f; float wY = screenH - 80.0f;
        float alphaN = (currentWeapon == WeaponType::Normal) ? 0.9f : 0.3f;
        RC::DrawBox({wX, wY}, {wX+20.0f, wY+20.0f}, {0.2f, 0.6f, 1.0f, alphaN});
        float alphaS = (currentWeapon == WeaponType::Spread) ? 0.9f : 0.3f;
        RC::DrawBox({wX+25.0f, wY}, {wX+45.0f, wY+20.0f}, {0.2f, 0.8f, 0.4f, alphaS});
        float alphaH = (currentWeapon == WeaponType::Heavy) ? 0.9f : 0.3f;
        RC::DrawBox({wX+50.0f, wY}, {wX+70.0f, wY+20.0f}, {0.8f, 0.3f, 0.1f, alphaH});

        // === Score display (tally circles) ===
        float scoreX = screenW - 220.0f;
        float scoreY = 30.0f;
        for (int i = 0; i < score && i < 20; ++i) {
            float cx = scoreX + (i % 10) * 20.0f;
            float cy = scoreY + (i / 10) * 20.0f;
            RC::DrawCircle({ cx, cy }, 7.0f, { 0.3f, 0.7f, 1.0f, 0.9f });
        }

        // === Game Over / Win overlay ===
        if (isDead) {
            RC::DrawBox({ 0.0f, screenH * 0.4f }, { screenW, screenH * 0.6f },
                        { 0.8f, 0.1f, 0.1f, 0.7f });
        }
    }

    void SpawnSplash(const RC::Vector3& pos, float impactSpeed, float weightVal) {
        Scene* scene = GetScene();
        if (!scene) return;

        float impactFactor = (impactSpeed * 0.1f) * (weightVal * 0.2f);
        if (impactFactor < 0.1f) impactFactor = 0.1f;
        if (impactFactor > 5.0f) impactFactor = 5.0f;

        // Strong water surface wave
        RC::WaveSource source;
        source.uv = RC::Vector2((pos.x / 100.0f) + 0.5f, (pos.z / 100.0f) + 0.5f);
        source.radius = 0.05f + (impactFactor * 0.02f);
        source.strength = 0.5f + (impactFactor * 0.5f);
        RC::AddWaveSource(source);

        // Spawn visual splash particles
        int splashCount = static_cast<int>(8 + (impactFactor * 8));
        if (splashCount > 30) splashCount = 30; // Max

        for (int i = 0; i < splashCount; ++i) {
            auto splash = scene->CreateEntity("Splash");

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

REGISTER_SCRIPT(PlayerController)
