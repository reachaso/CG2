#include "EnemyBaseScript.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/CameraComponent.h"
#include "Scene.h"
#include <iostream>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

enum class SharkState {
    Wait,
    Approach, // For future use
    Attack    // For future use
};

/// @brief Shark AI: waits in the distance for now
class SharkEnemyScript : public EnemyBaseScript {
public:
    float detectDistance = 30.0f;

protected:
    nlohmann::json Serialize() override {
        nlohmann::json j = EnemyBaseScript::Serialize(); // EnemyBaseScriptにSerializeがない場合は後でエラーになるかも？今回は空jsonとマージする形にする
        j["detectDistance"] = detectDistance;
        j["swimSpeed"] = swimSpeed;
        j["swimSineAmplitude"] = swimSineAmplitude;
        j["swimSineFrequency"] = swimSineFrequency;
        return j;
    }

    void Deserialize(const nlohmann::json& j) override {
        EnemyBaseScript::Deserialize(j);
        if (j.contains("detectDistance")) detectDistance = j["detectDistance"].get<float>();
        if (j.contains("swimSpeed")) swimSpeed = j["swimSpeed"].get<float>();
        if (j.contains("swimSineAmplitude")) swimSineAmplitude = j["swimSineAmplitude"].get<float>();
        if (j.contains("swimSineFrequency")) swimSineFrequency = j["swimSineFrequency"].get<float>();
    }

    void OnCreate() override {
        EnemyBaseScript::OnCreate();
        std::cout << "[SharkEnemyScript] OnCreate\n";
        state_ = SharkState::Wait;
        hp = 30; // サメは少し硬め
        maxHp = 30;
    }

    void OnUpdate(float deltaTime) override {
        EnemyBaseScript::OnUpdate(deltaTime);

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        if (isDead) {
            // 仮の沈む処理
            tr->position.y -= 2.0f * deltaTime;
            return;
        }

        Scene* scene = GetScene();
        if (!scene) return;

        // Find Camera (cached)
        std::shared_ptr<Entity> mainCamera = cachedTarget_.lock();
        if (!mainCamera || mainCamera->IsPendingDestroy() || !mainCamera->IsActive()) {
            mainCamera = nullptr;
            for (auto& e : scene->GetEntities()) {
                if (e->HasComponent<CameraComponent>()) {
                    mainCamera = e;
                    cachedTarget_ = mainCamera;
                    break;
                }
            }
        }
        if (!mainCamera) return;

        auto* camTr = mainCamera->GetComponent<TransformComponent>();
        if (!camTr) return;

        RC::Vector3 toCam = {
            camTr->position.x - tr->position.x,
            camTr->position.y - tr->position.y,
            camTr->position.z - tr->position.z
        };
        float distToCam = std::sqrt(toCam.x * toCam.x + toCam.z * toCam.z);

        // State Machine
        switch (state_) {
            case SharkState::Wait:
                // プレイヤーが一定距離に入ったらアクティブ(Approach)になる
                if (distToCam <= detectDistance) {
                    state_ = SharkState::Approach;
                    std::cout << "[SharkEnemyScript] Detected player! Switching to Approach.\n";
                }
                break;
            case SharkState::Approach:
                // アクティブ時にプレイヤーの方向を向く（LookAt計算）
                if (distToCam > 0.01f) {
                    tr->rotation.y = std::atan2(toCam.x, toCam.z);
                    
                    // Normalize direction
                    RC::Vector3 forward = { toCam.x / distToCam, 0.0f, toCam.z / distToCam };
                    // Right vector for sine wave (perpendicular to forward in XZ plane)
                    RC::Vector3 right = { forward.z, 0.0f, -forward.x };

                    swimTime_ += deltaTime;
                    
                    // Base forward movement
                    RC::Vector3 velocity = {
                        forward.x * swimSpeed,
                        0.0f,
                        forward.z * swimSpeed
                    };

                    // Add lateral sine wave movement (derivative of sin(t * freq) * amp)
                    float lateralVel = std::cos(swimTime_ * swimSineFrequency) * swimSineAmplitude * swimSineFrequency;
                    velocity.x += right.x * lateralVel;
                    velocity.z += right.z * lateralVel;

                    tr->position.x += velocity.x * deltaTime;
                    tr->position.z += velocity.z * deltaTime;
                }
                break;
            case SharkState::Attack:
                // アクティブ時にプレイヤーの方向を向く
                if (distToCam > 0.01f) {
                    tr->rotation.y = std::atan2(toCam.x, toCam.z);
                }
                break;
        }
    }

    void OnDestroy() override {
        std::cout << "[SharkEnemyScript] OnDestroy\n";
    }

public:
    void OnImGui() override {
        EnemyBaseScript::OnImGui();
#if RC_ENABLE_IMGUI
        ImGui::DragFloat("Detect Dist##Shark", &detectDistance, 0.5f, 5.0f, 100.0f);
        ImGui::DragFloat("Swim Speed##Shark", &swimSpeed, 0.5f, 1.0f, 50.0f);
        ImGui::DragFloat("Swim Sine Amp##Shark", &swimSineAmplitude, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Swim Sine Freq##Shark", &swimSineFrequency, 0.1f, 0.1f, 10.0f);
#endif
    }

private:
    SharkState state_ = SharkState::Wait;
    std::weak_ptr<Entity> cachedTarget_;
    
    // Swim parameters
    float swimSpeed = 8.0f;
    float swimSineAmplitude = 3.0f;
    float swimSineFrequency = 2.0f;
    float swimTime_ = 0.0f;
};

REGISTER_SCRIPT(SharkEnemyScript)
