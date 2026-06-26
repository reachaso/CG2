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

protected:
    void OnUpdate(float deltaTime) override {
        auto* input = Input::GetInstance();
        if (!input) return;

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
            currentCooldown = fireCooldown;
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

        // Raycast計算
        RC::Ray ray = RC::CameraMath::ScreenPointToRay(
            cursorPosition, 
            {screenW, screenH}, 
            ctx.View(), 
            ctx.Proj()
        );

        Scene* scene = GetScene();
        if (!scene) return;

        // 弾の生成
        auto bullet = scene->CreateEntity("PlayerBullet");
        
        // Transformの設定（カメラの近傍から発射する）
        auto* tr = &bullet->AddComponent<TransformComponent>();
        tr->position = ray.origin;
        tr->scale = { 0.3f, 0.3f, 0.3f };
        
        // メッシュの追加（描画用）
        auto* pm = &bullet->AddComponent<PrimitiveMeshComponent>();
        pm->type = PrimitiveType::Sphere;
        pm->meshHandle = RC::GenerateSphere(1.0f);
        if (pm->meshHandle >= 0) {
            if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                mat->color = { 0.2f, 0.6f, 1.0f, 0.85f };
            }
        }
        
        // パラメータをTagとして渡す (小数点以下を保持するためにスケールして整数化)
        bullet->SetTag("dir_x", static_cast<int>(ray.direction.x * 1000.0f));
        bullet->SetTag("dir_y", static_cast<int>(ray.direction.y * 1000.0f));
        bullet->SetTag("dir_z", static_cast<int>(ray.direction.z * 1000.0f));
        bullet->SetTag("bullet_speed", static_cast<int>(bulletSpeed * 10.0f)); 
        bullet->SetTag("bullet_lifetime", static_cast<int>(bulletLifetime * 10.0f));

        // Scriptのアタッチ
        auto* nsc = &bullet->AddComponent<NativeScriptComponent>();
        nsc->Bind("WaterBullet");
        nsc->SetScene(scene);
        if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());

        // ランタイムでの初期化
        scene->InitDynamicEntityRuntime(*bullet);

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
    }

public:
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
#endif
    }
};

REGISTER_SCRIPT(RailShooterController)
