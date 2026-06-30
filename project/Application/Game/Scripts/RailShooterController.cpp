#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "Engine/Input/Input.h"
#include "Engine/Input/Controller/Controller.h"
#include "RenderCommon.h"
#include "Engine/Render/RenderContext.h"
#include "Common/Math/MathUtils.h"
#include "Application/Framework/App.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include "Engine/Camera/CameraMath.h"
#include "ECS/TransformComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "Scene.h"

/// @brief レールシューティングにおける照準（レティクル）操作を担うスクリプト
class RailShooterController : public ScriptableEntity {
public:
    RC::Vector2 cursorPosition = { 640.0f, 360.0f }; // 初期位置（画面中央付近）
    
    // 感度調整用パラメータ
    float mouseSensitivity = 1.0f;
    float controllerSensitivity = 800.0f; // deltaTimeがかかるため大きめ
    float deadzone = 0.2f; // アナログスティックのデッドゾーン（20%）
    
    // 描画設定（仮）
    RC::Vector4 reticleColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    float reticleSize = 10.0f;

    // 射撃パラメータ
    float fireCooldown = 0.2f;
    float currentCooldown = 0.0f;
    float bulletSpeed = 40.0f;
    float bulletLifetime = 3.0f;

    // 演出用パラメータ
    float currentRecoil = 0.0f; // 射撃時のレティクル拡大量
    float recoilMax = 15.0f;    // 射撃時の最大反動量
    float recoilRecoverySpeed = 10.0f; // 反動の戻る速度

    float currentCameraShake = 0.0f; // カメラシェイクの現在強度
    float shakeMax = 0.5f;           // カメラシェイクの最大強度
    float shakeRecoverySpeed = 5.0f; // カメラシェイクの減衰速度

    // プレイヤーステータス
    int hp = 5;
    int maxHp = 5;
    float invincibleTimer = 0.0f;
    float invincibleDuration = 1.0f;
    bool isDead = false;
    int score = 0;

    enum class WeaponType { Normal, Spread, Heavy };
    WeaponType currentWeapon = WeaponType::Normal;

private:
    uint64_t bulletsFolderGuid_ = 0;

    uint64_t GetBulletsFolder(Scene* scene) {
        if (bulletsFolderGuid_ != 0) return bulletsFolderGuid_;
        for (auto& e : scene->GetEntities()) {
            if (e->GetName() == "PlayerBullets" && e->IsFolder()) {
                bulletsFolderGuid_ = e->Guid();
                return bulletsFolderGuid_;
            }
        }
        auto folder = scene->CreateEntity("PlayerBullets");
        folder->SetIsFolder(true);
        bulletsFolderGuid_ = folder->Guid();
        return bulletsFolderGuid_;
    }

protected:
    void OnCreate() override {
        if (Entity* self = GetEntity()) {
            self->SetTag("is_player", 1);
        }
    }

    void OnUpdate(float deltaTime) override {
        auto* input = Input::GetInstance();
        if (!input) return;

        // === 無敵タイマー ===
        if (invincibleTimer > 0.0f) {
            invincibleTimer -= deltaTime;
        }

        // === ダメージ・スコア処理 ===
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

        // Weapon Switching
        long wheel = input->GetMouseZ();
        if (wheel > 0) {
            int w = static_cast<int>(currentWeapon) - 1;
            if (w < 0) w = 2;
            currentWeapon = static_cast<WeaponType>(w);
        } else if (wheel < 0) {
            int w = static_cast<int>(currentWeapon) + 1;
            if (w > 2) w = 0;
            currentWeapon = static_cast<WeaponType>(w);
        }

        if (input->IsXInputConnected()) {
            if (input->IsXInputButtonTrigger(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
                int w = static_cast<int>(currentWeapon) - 1;
                if (w < 0) w = 2;
                currentWeapon = static_cast<WeaponType>(w);
            }
            if (input->IsXInputButtonTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
                int w = static_cast<int>(currentWeapon) + 1;
                if (w > 2) w = 0;
                currentWeapon = static_cast<WeaponType>(w);
            }
        }

        // デッドゾーン計算用の定数（GetXInputThumbLX/LYは-32768～32767）
        const float MAX_THUMB = 32767.0f;

        // 1. コントローラー入力の取得
        float stickX = input->GetXInputThumbLX() / MAX_THUMB;
        float stickY = -input->GetXInputThumbLY() / MAX_THUMB; // 画面上方向がマイナスYになることが多い場合反転調整が必要だが、ここでは一般的な2D画面座標系（Y下方向正）に合わせるよう調整

        // デッドゾーン処理
        if (std::abs(stickX) < deadzone) stickX = 0.0f;
        if (std::abs(stickY) < deadzone) stickY = 0.0f;

        // 2. マウス入力の取得
        float deltaMouseX = static_cast<float>(input->GetMouseX());
        float deltaMouseY = static_cast<float>(input->GetMouseY());

        float absMouseX = 0.0f, absMouseY = 0.0f;
        input->GetGameMousePosition(absMouseX, absMouseY);

        // 3. カーソル移動の適用
        // マウスの移動やクリックがあれば、マウス操作とみなして絶対座標にスナップする
        bool isMouseActive = (deltaMouseX != 0.0f || deltaMouseY != 0.0f || 
                              input->IsMousePressed(0) || input->IsMousePressed(1) || input->IsMousePressed(2));

        if (isMouseActive) {
            cursorPosition.x = absMouseX;
            cursorPosition.y = absMouseY;
        } else {
            // コントローラーのスティックは傾き続けるため deltaTime に依存させる
            cursorPosition.x += (stickX * controllerSensitivity * deltaTime);
            cursorPosition.y += (stickY * controllerSensitivity * deltaTime);
        }

        // 4. 画面外に出ないよう Clamp 処理
        auto& ctx = RC::GetRenderContext();
        float screenW = 1280.0f;
        float screenH = 720.0f;
        if (ctx.Ctx() && ctx.Ctx()->app) {
            screenW = static_cast<float>(ctx.Ctx()->app->width);
            screenH = static_cast<float>(ctx.Ctx()->app->height);
        }

        cursorPosition.x = RC::Clamp(cursorPosition.x, 0.0f, screenW);
        cursorPosition.y = RC::Clamp(cursorPosition.y, 0.0f, screenH);

        // 5. 射撃とクールダウン処理
        currentCooldown -= deltaTime;
        
        bool isFirePressed = input->IsMousePressed(0) || input->GetXInputRightTrigger() > 128;
        if (isFirePressed && currentCooldown <= 0.0f) {
            if (currentWeapon == WeaponType::Heavy) currentCooldown = fireCooldown * 2.0f;
            else currentCooldown = fireCooldown;
            FireBullet();
        }

        // 6. 演出（反動とシェイク）の更新
        if (currentRecoil > 0.0f) {
            currentRecoil -= recoilMax * recoilRecoverySpeed * deltaTime;
            if (currentRecoil < 0.0f) currentRecoil = 0.0f;
        }

        if (currentCameraShake > 0.0f) {
            currentCameraShake -= shakeMax * shakeRecoverySpeed * deltaTime;
            if (currentCameraShake < 0.0f) currentCameraShake = 0.0f;
        }

        // シーン内のカメラを探してShakeOffsetを適用
        if (Scene* scene = GetScene()) {
            for (auto& e : scene->GetEntities()) {
                if (auto* cam = e->GetComponent<CameraComponent>()) {
                    if (currentCameraShake > 0.0f) {
                        float sx = ((rand() % 100) / 50.0f - 1.0f) * currentCameraShake;
                        float sy = ((rand() % 100) / 50.0f - 1.0f) * currentCameraShake;
                        float sz = ((rand() % 100) / 50.0f - 1.0f) * currentCameraShake;
                        cam->shakeOffset = {sx, sy, sz};
                    } else {
                        cam->shakeOffset = {0.0f, 0.0f, 0.0f};
                    }
                    break;
                }
            }
        }
    }
    
    void FireBullet() {
        auto& ctx = RC::GetRenderContext();
        float screenW = 1280.0f;
        float screenH = 720.0f;
        if (ctx.Ctx() && ctx.Ctx()->app) {
            screenW = static_cast<float>(ctx.Ctx()->app->width);
            screenH = static_cast<float>(ctx.Ctx()->app->height);
        }

        int numBullets = (currentWeapon == WeaponType::Spread) ? 3 : 1;

        std::vector<std::shared_ptr<Entity>> inactiveBullets;
        if (Scene* scene = GetScene()) {
            for (auto& e : scene->GetEntities()) {
                if (e->GetName() == "PlayerBullet" && !e->IsActive() && !e->IsPendingDestroy()) {
                    inactiveBullets.push_back(e);
                    if (inactiveBullets.size() >= numBullets) break;
                }
            }
        }

        for (int i = 0; i < numBullets; ++i) {
            RC::Vector2 targetPos = cursorPosition;
            if (currentWeapon == WeaponType::Spread) {
                targetPos.x += (i - 1) * 80.0f; // -80, 0, 80 pixels offset
            }

            // Raycast計算
            RC::Ray ray = RC::CameraMath::ScreenPointToRay(
                targetPos, 
                {screenW, screenH}, 
                ctx.View(), 
                ctx.Proj()
            );

            Scene* scene = GetScene();
            if (!scene) continue;

            // Create bullet entity (Pooling)
            std::shared_ptr<Entity> bullet = nullptr;
            bool isNew = false;
            if (i < inactiveBullets.size()) {
                bullet = inactiveBullets[i];
                bullet->SetActive(true);
                bullet->SetTag("reused", 1);
            } else {
                bullet = scene->CreateEntity("PlayerBullet");
                isNew = true;
            }

            auto* tr = bullet->GetComponent<TransformComponent>();
            if (!tr) tr = &bullet->AddComponent<TransformComponent>();
            tr->position = ray.origin;
            bullet->SetParentGuid(GetBulletsFolder(scene));
            float scale = 0.3f;
            if (currentWeapon == WeaponType::Heavy) scale = 0.6f;
            else if (currentWeapon == WeaponType::Spread) scale = 0.2f;
            tr->scale = { scale, scale, scale };
            
            // メッシュの追加（描画用）
            auto* pm = bullet->GetComponent<PrimitiveMeshComponent>();
            if (!pm) {
                pm = &bullet->AddComponent<PrimitiveMeshComponent>();
                pm->type = PrimitiveType::Sphere;
                pm->meshHandle = RC::GenerateSphere(1.0f);
            }
            if (pm->meshHandle >= 0) {
                if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                    mat->color = { 0.2f, 0.6f, 1.0f, 0.85f };
                }
            }
            
            // パラメータをTagとして渡す
            bullet->SetTag("dir_x", static_cast<int>(ray.direction.x * 1000.0f));
            bullet->SetTag("dir_y", static_cast<int>(ray.direction.y * 1000.0f));
            bullet->SetTag("dir_z", static_cast<int>(ray.direction.z * 1000.0f));
            bullet->SetTag("bullet_speed", static_cast<int>(bulletSpeed * 10.0f)); 
            bullet->SetTag("bullet_lifetime", static_cast<int>(bulletLifetime * 10.0f));

            if (currentWeapon == WeaponType::Normal) bullet->SetTag("bullet_type", 0);
            else if (currentWeapon == WeaponType::Spread) bullet->SetTag("bullet_type", 1);
            else if (currentWeapon == WeaponType::Heavy) bullet->SetTag("bullet_type", 2);

            // Scriptのアタッチ
            auto* nsc = bullet->GetComponent<NativeScriptComponent>();
            if (!nsc) {
                nsc = &bullet->AddComponent<NativeScriptComponent>();
                nsc->AddScript("WaterBullet");
            }
            nsc->SetScene(scene);
            if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());

            if (isNew) {
                scene->InitDynamicEntityRuntime(*bullet);
            } else {
                // PrimitiveMeshのTransformを即座に同期（チラつき防止）
                if (pm && pm->meshHandle >= 0) {
                    if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                        pmTr->translation = tr->position;
                    }
                }
            }
        }

        // 演出のトリガー
        currentRecoil = recoilMax;
        currentCameraShake = shakeMax;
    }
    
    void OnRender() override {
        // Primitive 2D描画を用いてレティクルを描画する
        RC::DrawCircle(cursorPosition, reticleSize + currentRecoil, reticleColor, kFill, 1.0f);
        
        // 中心点を描画（見やすくするため黒の四角形）
        RC::DrawBox({cursorPosition.x - 2.0f, cursorPosition.y - 2.0f}, 
                    {cursorPosition.x + 2.0f, cursorPosition.y + 2.0f}, 
                    {0.0f, 0.0f, 0.0f, 1.0f});

        // Weapon Indicator
        auto& ctx = RC::GetRenderContext();
        float screenH = 720.0f;
        if (ctx.Ctx() && ctx.Ctx()->app) {
            screenH = static_cast<float>(ctx.Ctx()->app->height);
        }
        float wX = 20.0f; float wY = screenH - 80.0f;
        float alphaN = (currentWeapon == WeaponType::Normal) ? 0.9f : 0.3f;
        RC::DrawBox({wX, wY}, {wX+20.0f, wY+20.0f}, {0.2f, 0.6f, 1.0f, alphaN});
        float alphaS = (currentWeapon == WeaponType::Spread) ? 0.9f : 0.3f;
        RC::DrawBox({wX+25.0f, wY}, {wX+45.0f, wY+20.0f}, {0.2f, 0.8f, 0.4f, alphaS});
        float alphaH = (currentWeapon == WeaponType::Heavy) ? 0.9f : 0.3f;
        RC::DrawBox({wX+50.0f, wY}, {wX+70.0f, wY+20.0f}, {0.8f, 0.3f, 0.1f, alphaH});

        // === Player HP Bar ===
        float screenW = 1280.0f;
        if (ctx.Ctx() && ctx.Ctx()->app) {
            screenW = static_cast<float>(ctx.Ctx()->app->width);
        }
        float barX = 20.0f;
        float barY = screenH - 50.0f;
        float barW = 200.0f;
        float barH = 20.0f;

        // Background
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH }, { 0.3f, 0.05f, 0.05f, 0.8f });

        // Foreground
        float hpRatio = static_cast<float>(hp) / static_cast<float>(maxHp);
        RC::Vector4 hpColor = { 1.0f - hpRatio, hpRatio, 0.1f, 0.9f };
        RC::DrawBox({ barX, barY }, { barX + barW * hpRatio, barY + barH }, hpColor);

        // Border
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH }, { 1.0f, 1.0f, 1.0f, 0.5f }, kWire);

        // === Score display (tally circles) ===
        float scoreX = screenW - 220.0f;
        float scoreY = 30.0f;
        for (int i = 0; i < score && i < 20; ++i) {
            float cx = scoreX + (i % 10) * 20.0f;
            float cy = scoreY + (i / 10) * 20.0f;
            RC::DrawCircle({ cx, cy }, 7.0f, { 0.3f, 0.7f, 1.0f, 0.9f });
        }

        // === Game Over / Damage overlay ===
        if (isDead) {
            RC::DrawBox({ 0.0f, screenH * 0.4f }, { screenW, screenH * 0.6f },
                        { 0.8f, 0.1f, 0.1f, 0.7f });
        } else if (invincibleTimer > 0.0f) {
            float flashAlpha = (invincibleTimer / invincibleDuration) * 0.5f;
            RC::DrawBox({ 0.0f, 0.0f }, { screenW, screenH },
                        { 1.0f, 0.0f, 0.0f, flashAlpha });
        }
    }

public:
    void TakeDamage(int damage) {
        if (invincibleTimer > 0.0f || isDead) return;
        hp -= damage;
        invincibleTimer = invincibleDuration;
        
        // 演出
        currentCameraShake = shakeMax * 2.0f;

        if (hp <= 0) {
            hp = 0;
            isDead = true;
            if (Entity* self = GetEntity()) {
                self->SetTag("game_over", 1);
            }
        }
    }

    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::Text("Cursor Pos: (%.1f, %.1f)", cursorPosition.x, cursorPosition.y);
        
        ImGui::Separator();
        ImGui::Text("Sensitivity Settings");
        ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity, 0.1f, 0.1f, 10.0f);
        ImGui::DragFloat("Controller Sensitivity", &controllerSensitivity, 10.0f, 10.0f, 2000.0f);
        ImGui::SliderFloat("Deadzone", &deadzone, 0.0f, 1.0f);
        
        ImGui::Separator();
        ImGui::Text("Reticle Settings");
        ImGui::ColorEdit4("Color", &reticleColor.x);
        ImGui::DragFloat("Size", &reticleSize, 0.5f, 1.0f, 100.0f);

        ImGui::Separator();
        ImGui::Text("Weapon Settings");
        ImGui::DragFloat("Fire Cooldown", &fireCooldown, 0.05f, 0.05f, 5.0f);
        ImGui::DragFloat("Bullet Speed", &bulletSpeed, 1.0f, 10.0f, 200.0f);
        ImGui::DragFloat("Bullet Lifetime", &bulletLifetime, 0.1f, 0.5f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Player Stats");
        ImGui::DragInt("HP", &hp, 1, 0, maxHp);
        ImGui::Text("Score: %d", score);
#endif
    }
};

REGISTER_SCRIPT(RailShooterController)
