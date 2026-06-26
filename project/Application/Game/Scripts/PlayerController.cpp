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
    float invincibleTimer = 0.0f;
    float invincibleDuration = 1.0f;
    int score = 0;
    float weight = 5.0f;
    bool isDead = false;

    protected:
    void OnCreate() override {
        std::cout << "[PlayerController] OnCreate\n";
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

private:
    RC::Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    bool firstUpdate_ = true;
    float verticalVel_ = 0.0f;

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

            splash->SetTag("impact_factor", static_cast<int>(impactFactor * 100));

            auto* tr = splash->GetComponent<TransformComponent>();
            if (!tr) tr = &splash->AddComponent<TransformComponent>();
            tr->position = pos;
            
            float s = (0.15f + (i % 4) * 0.05f) * (1.0f + impactFactor * 0.3f);
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

            // 即座に PrimitiveMesh の Transform を同期して原点でのチラつきを防ぐ
            if (pm->meshHandle >= 0) {
                if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                    pmTr->scale = tr->scale;
                    pmTr->rotation = tr->rotation;
                    pmTr->translation = tr->position;
                }
            }
        }
    }
};

REGISTER_SCRIPT(PlayerController)
