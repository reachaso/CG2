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
    Approach,
    Attack,
    Cooldown
};

/// @brief Shark AI: waits in the distance for now
class SharkEnemyScript : public EnemyBaseScript {
public:
    float detectDistance = 30.0f;
    float attackStartDistance = 10.0f;
    float attackHitDistance = 2.5f;
    float attackSpeed = 25.0f;
    float cooldownDuration = 2.0f;
    RC::Vector3 modelRotationOffsetDeg = { 180.0f, 80.0f, 180.0f }; // モデルの初期回転・向き補正（度数法）

protected:
    nlohmann::json Serialize() override {
        nlohmann::json j = EnemyBaseScript::Serialize(); // EnemyBaseScriptにSerializeがない場合は後でエラーになるかも？今回は空jsonとマージする形にする
        j["detectDistance"] = detectDistance;
        j["swimSpeed"] = swimSpeed;
        j["swimSineAmplitude"] = swimSineAmplitude;
        j["swimSineFrequency"] = swimSineFrequency;
        j["attackStartDistance"] = attackStartDistance;
        j["attackHitDistance"] = attackHitDistance;
        j["attackSpeed"] = attackSpeed;
        j["cooldownDuration"] = cooldownDuration;
        j["modelRotOffset"] = { modelRotationOffsetDeg.x, modelRotationOffsetDeg.y, modelRotationOffsetDeg.z };
        return j;
    }

    void Deserialize(const nlohmann::json& j) override {
        EnemyBaseScript::Deserialize(j);
        if (j.contains("detectDistance")) detectDistance = j["detectDistance"].get<float>();
        if (j.contains("swimSpeed")) swimSpeed = j["swimSpeed"].get<float>();
        if (j.contains("swimSineAmplitude")) swimSineAmplitude = j["swimSineAmplitude"].get<float>();
        if (j.contains("swimSineFrequency")) swimSineFrequency = j["swimSineFrequency"].get<float>();
        if (j.contains("attackStartDistance")) attackStartDistance = j["attackStartDistance"].get<float>();
        if (j.contains("attackHitDistance")) attackHitDistance = j["attackHitDistance"].get<float>();
        if (j.contains("attackSpeed")) attackSpeed = j["attackSpeed"].get<float>();
        if (j.contains("cooldownDuration")) cooldownDuration = j["cooldownDuration"].get<float>();
        if (j.contains("modelRotOffset") && j["modelRotOffset"].size() == 3) {
            modelRotationOffsetDeg.x = j["modelRotOffset"][0].get<float>();
            modelRotationOffsetDeg.y = j["modelRotOffset"][1].get<float>();
            modelRotationOffsetDeg.z = j["modelRotOffset"][2].get<float>();
        }
    }

    void OnCreate() override {
        EnemyBaseScript::OnCreate();
        std::cout << "[SharkEnemyScript] OnCreate\n";
        state_ = SharkState::Wait;
        hp = 20; // サメのHP調整
        maxHp = 20;

        if (auto* tr = GetComponent<TransformComponent>()) {
            tr->rotation.x = modelRotationOffsetDeg.x * (3.14159265f / 180.0f);
            tr->rotation.y = modelRotationOffsetDeg.y * (3.14159265f / 180.0f);
            tr->rotation.z = modelRotationOffsetDeg.z * (3.14159265f / 180.0f);
        }
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
                } else {
                    // 待機中はその場で円を描いてパトロール
                    swimTime_ += deltaTime;
                    float patrolSpeed = swimSpeed * 0.5f;
                    tr->rotation.y += patrolSpeed * 0.1f * deltaTime; 
                    float realAngleY = tr->rotation.y - (modelRotationOffsetDeg.y * (3.14159265f / 180.0f));
                    tr->position.x += std::sin(realAngleY) * patrolSpeed * deltaTime;
                    tr->position.z += std::cos(realAngleY) * patrolSpeed * deltaTime;
                }
                break;
            case SharkState::Approach:
                // アクティブ時にプレイヤーの方向を向く（LookAt計算）
                if (distToCam > 0.01f) {
                    float targetAngleY = std::atan2(toCam.x, toCam.z);
                    tr->rotation.y = targetAngleY + (modelRotationOffsetDeg.y * (3.14159265f / 180.0f));
                    // xとzはOnCreateで設定したオフセットを維持
                    
                    // 進行方向（補正前）のベクトル
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

                // 攻撃開始距離に入ったらAttack状態へ
                if (distToCam <= attackStartDistance) {
                    state_ = SharkState::Attack;
                    attackTimer_ = 0.0f;
                    hasHit_ = false;
                    std::cout << "[SharkEnemyScript] Approaching -> Attack!\n";
                }
                break;
            case SharkState::Attack:
                // 追尾（回転）をやめ、現在の向きに直進して突進する
                {
                    // 実際の進行方向ベクトルをrotation.yから逆算（モデルのオフセット分を引く）
                    float realAngleY = tr->rotation.y - (modelRotationOffsetDeg.y * (3.14159265f / 180.0f));
                    float s = std::sin(realAngleY);
                    float c = std::cos(realAngleY);
                    RC::Vector3 forward = { s, 0.0f, c };
                    
                    tr->position.x += forward.x * attackSpeed * deltaTime;
                    tr->position.z += forward.z * attackSpeed * deltaTime;
                }
                
                attackTimer_ += deltaTime;

                // 攻撃ヒット距離に入ったらダメージ処理（1回のみ）
                if (!hasHit_ && distToCam <= attackHitDistance) {
                    std::cout << "[SharkEnemyScript] Player HIT! Damage triggered.\n";
                    mainCamera->SetTag("pending_damage", 1);
                    hasHit_ = true;
                }
                
                // タイムアウトでクールダウンへ（通り過ぎるのを待つ）
                if (attackTimer_ >= maxAttackDuration) {
                    std::cout << "[SharkEnemyScript] Attack Finished! To Cooldown.\n";
                    state_ = SharkState::Cooldown;
                    cooldownTimer_ = cooldownDuration;
                }
                break;
            case SharkState::Cooldown:
                // クールダウン中は高速で逃げる（距離をとり直すため）
                if (distToCam > 0.01f) {
                    RC::Vector3 backward = { -toCam.x / distToCam, 0.0f, -toCam.z / distToCam };
                    tr->position.x += backward.x * (swimSpeed * 1.5f) * deltaTime;
                    tr->position.z += backward.z * (swimSpeed * 1.5f) * deltaTime;

                    // 逃げる方向を向かせる
                    float targetAngleY = std::atan2(-toCam.x, -toCam.z);
                    tr->rotation.y = targetAngleY + (modelRotationOffsetDeg.y * (3.14159265f / 180.0f));
                }
                
                cooldownTimer_ -= deltaTime;
                if (cooldownTimer_ <= 0.0f) {
                    // クールダウン終了後、再びApproachに戻るか距離を見て判断
                    if (distToCam <= detectDistance) {
                        state_ = SharkState::Approach;
                    } else {
                        state_ = SharkState::Wait;
                    }
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
        ImGui::DragFloat("Attack Start Dist##Shark", &attackStartDistance, 0.5f, 2.0f, 50.0f);
        ImGui::DragFloat("Attack Hit Dist##Shark", &attackHitDistance, 0.1f, 0.5f, 10.0f);
        ImGui::DragFloat("Attack Speed##Shark", &attackSpeed, 0.5f, 1.0f, 100.0f);
        ImGui::DragFloat("Cooldown Dur##Shark", &cooldownDuration, 0.1f, 0.5f, 10.0f);
        ImGui::DragFloat3("Model Rot Offset##Shark", &modelRotationOffsetDeg.x, 1.0f, -360.0f, 360.0f);
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
    float cooldownTimer_ = 0.0f;
    float attackTimer_ = 0.0f;
    float maxAttackDuration = 2.0f;
    bool hasHit_ = false;
};

REGISTER_SCRIPT(SharkEnemyScript)
