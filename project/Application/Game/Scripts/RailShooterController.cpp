#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "Input/Input.h"
#include "Engine/Input/Controller/Controller.h"
#include "RenderCommon.h"
#include "Engine/Render/RenderContext.h"
#include "Common/Math/MathUtils.h"
#include "Application/Framework/App.h"
#include "Common/Log/Log.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include "Engine/Camera/CameraMath.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "ECS/TransformComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/ColliderComponent.h"
#include "Scene.h"
#include <algorithm>
#include <utility>
#include <cmath>

namespace {
    bool IntersectSegmentAABB(const RC::Vector3& p0, const RC::Vector3& p1, const RC::Vector3& min, const RC::Vector3& max) {
        RC::Vector3 d = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
        float tmin = 0.0f;
        float tmax = 1.0f;
        
        // X axis
        if (std::abs(d.x) < 0.00001f) {
            if (p0.x < min.x || p0.x > max.x) return false;
        } else {
            float ood = 1.0f / d.x;
            float t1 = (min.x - p0.x) * ood;
            float t2 = (max.x - p0.x) * ood;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
        // Y axis
        if (std::abs(d.y) < 0.00001f) {
            if (p0.y < min.y || p0.y > max.y) return false;
        } else {
            float ood = 1.0f / d.y;
            float t1 = (min.y - p0.y) * ood;
            float t2 = (max.y - p0.y) * ood;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
        // Z axis
        if (std::abs(d.z) < 0.00001f) {
            if (p0.z < min.z || p0.z > max.z) return false;
        } else {
            float ood = 1.0f / d.z;
            float t1 = (min.z - p0.z) * ood;
            float t2 = (max.z - p0.z) * ood;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
        return true;
    }

    bool RaycastTerrain(Scene* scene, const RC::Vector3& start, const RC::Vector3& end) {
        if (!scene) return false;
        for (auto& e : scene->GetEntities()) {
            if (e->GetTagInt("is_terrain", 0) == 1) {
                auto* col = e->GetComponent<ColliderComponent>();
                auto* tr = e->GetComponent<TransformComponent>();
                if (col && tr && col->IsEnabled() && col->shape == ColliderComponent::Shape::AABB) {
                    RC::Vector3 scaledSize = {
                        std::abs(col->size.x * tr->scale.x),
                        std::abs(col->size.y * tr->scale.y),
                        std::abs(col->size.z * tr->scale.z)
                    };
                    RC::Vector3 halfSize = { scaledSize.x * 0.5f, scaledSize.y * 0.5f, scaledSize.z * 0.5f };
                    RC::Vector3 center = {
                        tr->position.x + col->center.x * tr->scale.x,
                        tr->position.y + col->center.y * tr->scale.y,
                        tr->position.z + col->center.z * tr->scale.z
                    };
                    RC::Vector3 minPos = { center.x - halfSize.x, center.y - halfSize.y, center.z - halfSize.z };
                    RC::Vector3 maxPos = { center.x + halfSize.x, center.y + halfSize.y, center.z + halfSize.z };
                    
                    if (IntersectSegmentAABB(start, end, minPos, maxPos)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

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

    // 敵全体（ボス）情報
    int totalEnemyHp = 0;
    int totalEnemyMaxHp = 0;
    bool isGameCleared = false;

    // 水中状態フラグ
    bool isUnderwater = false;
    float transitionTimer = 0.0f;
    float transitionSpeed = 2.0f; // 0.5秒で完全に切り替わる
    
    RC::Vector4 underwaterFogColor = { 0.0f, 0.3f, 0.6f, 1.0f };
    float underwaterFogStart = 10.0f;
    float underwaterFogEnd = 150.0f;

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
        if (deltaTime <= 0.0f) return; // ゲームが一時停止・停止中の場合は処理しない

        auto* input = Input::GetInstance();
        if (!input) return;

        // === 無敵タイマー ===
        if (invincibleTimer > 0.0f) {
            invincibleTimer -= deltaTime;
        }

        // === 水中判定とトランジション ===
        if (auto* tr = GetComponent<TransformComponent>()) {
            bool currentUnderwater = (tr->position.y < 0.0f);
            if (currentUnderwater != isUnderwater) {
                isUnderwater = currentUnderwater;
                if (Entity* self = GetEntity()) {
                    self->SetTag("is_underwater", isUnderwater ? 1 : 0);
                }
                if (isUnderwater) {
                    Log::Print("[Event] Transition to Underwater (Dive)");
                    if (auto* postProcess = RC::GetRenderContext().GetPostProcess()) {
                        postProcess->AddEffect(PostEffectType::Underwater);
                    }
                } else {
                    Log::Print("[Event] Transition to Surface (Emerge)");
                }
            }
        }

        // トランジションの更新
        float targetTransition = isUnderwater ? 1.0f : 0.0f;
        if (transitionTimer != targetTransition) {
            if (transitionTimer < targetTransition) {
                transitionTimer += transitionSpeed * deltaTime;
                if (transitionTimer > targetTransition) transitionTimer = targetTransition;
            } else {
                transitionTimer -= transitionSpeed * deltaTime;
                if (transitionTimer < targetTransition) transitionTimer = targetTransition;
            }

            if (auto* postProcess = RC::GetRenderContext().GetPostProcess()) {
                // S字カーブ(SmoothStep)をかけても良いが、とりあえずLinear
                float smoothedLerp = transitionTimer * transitionTimer * (3.0f - 2.0f * transitionTimer);
                postProcess->SetUnderwaterLerpFactor(smoothedLerp);
                postProcess->SetUnderwaterFogColor(underwaterFogColor.x, underwaterFogColor.y, underwaterFogColor.z, underwaterFogColor.w);
                postProcess->SetUnderwaterFogRange(underwaterFogStart, underwaterFogEnd);
                
                // 完全に水上に戻りきったらエフェクト自体をRemoveする
                if (transitionTimer == 0.0f && !isUnderwater) {
                    postProcess->RemoveEffect(PostEffectType::Underwater);
                }
            }
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

        // 敵の合計HPの集計、ダメージ処理、およびクリア判定
        totalEnemyHp = 0;
        totalEnemyMaxHp = 0;
        if (Scene* scene = GetScene()) {
            for (auto& e : scene->GetEntities()) {
                if (e->GetTagInt("is_enemy", 0) == 1) {
                    // タグが未設定なら初期化
                    int ehp = e->GetTagInt("current_hp", -1);
                    int eMaxHp = e->GetTagInt("max_hp", -1);
                    if (ehp == -1) { ehp = 30; e->SetTag("current_hp", 30); }
                    if (eMaxHp == -1) { eMaxHp = 30; e->SetTag("max_hp", 30); }

                    totalEnemyHp += ehp;
                    totalEnemyMaxHp += eMaxHp;
                }
            }
        }
        if (totalEnemyMaxHp > 0 && totalEnemyHp <= 0) {
            isGameCleared = true;
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
        if (!isDead) {
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
        if (!isDead) {
            currentCooldown -= deltaTime;
            
            bool isFirePressed = false;
#if RC_ENABLE_IMGUI
            // ImGuiがマウスをキャプチャしていても、Viewport上なら射撃を許可する
            if (!ImGui::GetIO().WantCaptureMouse || input->IsViewportHovered()) {
                isFirePressed = input->IsMousePressed(0);
            }
#else
            isFirePressed = input->IsMousePressed(0);
#endif
            if (input->GetXInputRightTrigger() > 128) {
                isFirePressed = true;
            }

            if (isFirePressed && currentCooldown <= 0.0f) {
                if (currentWeapon == WeaponType::Heavy) currentCooldown = fireCooldown * 2.0f;
                else currentCooldown = fireCooldown;
                FireBullet();
            }
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
        
        bool drawPlayerUI = true;
        if (invincibleTimer > 0.0f) {
            if (static_cast<int>(invincibleTimer * 10.0f) % 2 == 0) {
                drawPlayerUI = false;
            }
        }

        if (drawPlayerUI) {
            RC::DrawBox({ barX, barY }, { barX + barW, barY + barH }, { 0.3f, 0.05f, 0.05f, 0.8f });

            // Foreground
            float hpRatio = static_cast<float>(hp) / static_cast<float>(maxHp);
            RC::Vector4 hpColor = { 1.0f - hpRatio, hpRatio, 0.1f, 0.9f };
            RC::DrawBox({ barX, barY }, { barX + barW * hpRatio, barY + barH }, hpColor);

            // Border
            RC::DrawBox({ barX, barY }, { barX + barW, barY + barH }, { 1.0f, 1.0f, 1.0f, 0.5f }, kWire);
        }



        // === 敵ごとの個別HPバーを頭上に描画 ===
        if (Scene* scene = GetScene()) {
            RC::Matrix4x4 viewProj = Multiply(ctx.View(), ctx.Proj());
            for (auto& e : scene->GetEntities()) {
                if (e->GetTagInt("is_enemy", 0) == 1) {
                    int ehp = e->GetTagInt("current_hp", 30);
                    int eMaxHp = e->GetTagInt("max_hp", 30);
                    if (ehp <= 0 || eMaxHp <= 0) continue;

                    auto* tr = e->GetComponent<TransformComponent>();
                    if (!tr) continue;
                    
                    // 頭の少し上を計算
                    RC::Vector3 headPos = tr->position;
                    headPos.y += 1.2f; 
                    
                    // w(深度)計算による背後判定
                    float w = headPos.x * viewProj.m[0][3] + headPos.y * viewProj.m[1][3] +
                              headPos.z * viewProj.m[2][3] + 1.0f * viewProj.m[3][3];
                    if (w < 0.1f) continue; 
                    
                    // 地形（岩など）による遮蔽チェック
                    RC::Vector3 cameraPos = { 0, 0, 0 };
                    if (auto* myTr = GetComponent<TransformComponent>()) {
                        cameraPos = myTr->position;
                    }
                    if (RaycastTerrain(scene, cameraPos, headPos)) {
                        continue; // 岩に隠れている場合はHPバーを描画しない
                    }
                    
                    // スクリーン座標へ変換
                    RC::Vector3 screenPos = RC::CameraMath::WorldToScreenPoint(
                        headPos, {screenW, screenH}, ctx.View(), ctx.Proj());
                        
                    // 画面外に飛ばないようにクランプ（画面端で見えるようにする）
                    screenPos.x = RC::Clamp(screenPos.x, 50.0f, screenW - 50.0f);
                    screenPos.y = RC::Clamp(screenPos.y, 50.0f, screenH - 50.0f);
                    
                    // 描画
                    float eBarW = 100.0f;
                    float eBarH = 10.0f;
                    float eBarX = screenPos.x - eBarW * 0.5f;
                    float eBarY = screenPos.y;
                    
                    RC::DrawBox({ eBarX, eBarY }, { eBarX + eBarW, eBarY + eBarH }, { 0.3f, 0.05f, 0.05f, 0.8f });
                    
                    float eHpRatio = static_cast<float>(ehp) / static_cast<float>(eMaxHp);
                    RC::DrawBox({ eBarX, eBarY }, { eBarX + eBarW * eHpRatio, eBarY + eBarH }, { 0.9f, 0.2f, 0.2f, 0.9f });
                    
                    RC::DrawBox({ eBarX, eBarY }, { eBarX + eBarW, eBarY + eBarH }, { 1.0f, 1.0f, 1.0f, 0.5f }, kWire);
                }
            }
        }

        // === Game Over / Game Clear / Damage overlay ===
        if (isDead) {
            RC::DrawBox({ 0.0f, screenH * 0.4f }, { screenW, screenH * 0.6f },
                        { 0.8f, 0.1f, 0.1f, 0.7f });
        } else if (isGameCleared) {
            RC::DrawBox({ 0.0f, screenH * 0.4f }, { screenW, screenH * 0.6f },
                        { 0.1f, 0.8f, 0.2f, 0.6f });
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
        if (isDead) {
            auto& ctx = RC::GetRenderContext();
            float screenW = 1280.0f;
            float screenH = 720.0f;
            if (ctx.Ctx() && ctx.Ctx()->app) {
                screenW = static_cast<float>(ctx.Ctx()->app->width);
                screenH = static_cast<float>(ctx.Ctx()->app->height);
            }
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("GameOverOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(4.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "GAME OVER");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }

        // --- HUD Overlay (Score) ---
        if (!isDead && !isGameCleared) {
            auto& ctx = RC::GetRenderContext();
            float screenW = 1280.0f;
            float screenH = 720.0f;
            if (ctx.Ctx() && ctx.Ctx()->app) {
                screenW = static_cast<float>(ctx.Ctx()->app->width);
                screenH = static_cast<float>(ctx.Ctx()->app->height);
            }
            ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("HUDOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(2.5f);
            
            // ドロップシャドウ風
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 2.5f, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 255), std::format("SCORE: {:06d}", score).c_str());
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "SCORE: %06d", score);
            
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }

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

        ImGui::Separator();
        ImGui::Text("Underwater Settings");
        ImGui::DragFloat("Transition Speed", &transitionSpeed, 0.1f, 0.1f, 10.0f);
        ImGui::ColorEdit4("Fog Color", &underwaterFogColor.x);
        ImGui::DragFloat("Fog Start", &underwaterFogStart, 1.0f, 0.0f, 50.0f);
        ImGui::DragFloat("Fog End", &underwaterFogEnd, 1.0f, 50.0f, 500.0f);
#endif
    }
};

REGISTER_SCRIPT(RailShooterController)
