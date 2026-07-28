#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/AnimationComponent.h"
#include "ECS/CameraComponent.h"
#include "Input/Input.h"
#include "Common/EngineConfig.h"
#include "RenderCommon.h"
#include "Scene.h"
#include <iostream>
#include <cmath>
#include <algorithm>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief 農民キャラクターの状態
enum class FarmerState {
    Idle1,   ///< 通常待機
    Idle2,   ///< 5秒放置後の待機
    Walk,    ///< 歩き
    Run,     ///< 走り
    Attack   ///< 切り付け
};

/// @brief 農民キャラクターコントローラー
/// WASD移動、SHIFT+WASDで走り、左クリックで攻撃、放置で待機アニメーション切り替え
class FarmerController : public ScriptableEntity {
public:
    // アニメーションインデックス（GLB内の順番）
    int animIdle1  = 4;  ///< 待機1 (通常待機)
    int animIdle2  = 1;  ///< 待機2 (5秒放置後の待機)
    int animWalk   = 0;  ///< 歩き
    int animRun    = 2;  ///< 走り
    int animAttack = 3;  ///< 切り付け

    float walkSpeed = 3.0f;        ///< 歩き速度
    float runSpeed  = 8.0f;        ///< 走り速度
    float rotationSpeed = 10.0f;   ///< 回転の滑らかさ
    float idleTimeout = 5.0f;      ///< 待機2に切り替わるまでの秒数
    float blendDuration = 0.2f;    ///< アニメーションブレンド時間

    // カメラ追従設定
    float cameraDistance = 8.0f;   ///< カメラ距離
    float cameraHeight = 3.0f;     ///< カメラ高さ
    float cameraLookHeight = 1.0f; ///< 注視点の高さオフセット
    float cameraDeadZoneX = 1.5f;      ///< 横方向のデッドゾーン幅（プレイヤーがこの範囲内にいる間はカメラのXを動かさない。範囲外に出たら瞬間的にズレなく追従する）

protected:
    void OnCreate() override {
        std::cout << "[FarmerController] OnCreate\n";
        state_ = FarmerState::Idle1;
        idleTimer_ = 0.0f;
        attackTimer_ = 0.0f;
        facingAngle_ = 0.0f;
        currentAnimIndex_ = -1; // 未設定
        isCameraInitialized_ = false;
        if (auto* tr = GetComponent<TransformComponent>()) {
            // 初期向き: 画面奥（+Z / Wキーの前進方向）を向かせる
            tr->rotation.y = facingAngle_;
        }
    }

    void OnUpdate(float deltaTime) override {
        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        auto* ren = GetComponent<ModelRendererComponent>();
        if (!ren || !ren->HasModel()) return;

        Input* input = Input::GetInstance();
        if (!input) return;

        // モデルのファイルパスを取得（アニメーション切り替え用）
        if (modelPath_.empty()) {
            modelPath_ = ren->modelPath;
        }

        // --- 入力の取得 ---
        float moveX = 0.0f;
        float moveZ = 0.0f;
        bool shiftHeld = false;
        bool attackTriggered = false;

        // W=前進（カメラから離れる＝画面奥方向）／S=後退（カメラに近づく＝画面手前方向）
        // カメラはプレイヤーの -Z 側に位置し Yaw=0（+Z を向く）ため、
        // 画面奥 = ワールド +Z、画面手前 = ワールド -Z となる
        if (input->IsKeyPressed(DIK_W)) moveZ += 1.0f;
        if (input->IsKeyPressed(DIK_S)) moveZ -= 1.0f;
        if (input->IsKeyPressed(DIK_D)) moveX += 1.0f;
        if (input->IsKeyPressed(DIK_A)) moveX -= 1.0f;

        shiftHeld = input->IsKeyPressed(DIK_LSHIFT) || input->IsKeyPressed(DIK_RSHIFT);
        attackTriggered = input->IsMouseTrigger(0);

        bool isMoving = (moveX != 0.0f || moveZ != 0.0f);

        // 入力ベクトルの正規化
        if (isMoving) {
            float len = std::sqrt(moveX * moveX + moveZ * moveZ);
            if (len > 1.0f) {
                moveX /= len;
                moveZ /= len;
            }
        }

        // --- ステートマシン ---
        FarmerState newState = state_;

        if (state_ == FarmerState::Attack) {
            // 攻撃中はタイマーで管理
            attackTimer_ -= deltaTime;
            if (attackTimer_ <= 0.0f) {
                newState = FarmerState::Idle1;
                idleTimer_ = 0.0f;
            }
        } else if (attackTriggered) {
            newState = FarmerState::Attack;
        } else if (isMoving) {
            if (shiftHeld) {
                newState = FarmerState::Run;
            } else {
                newState = FarmerState::Walk;
            }
            idleTimer_ = 0.0f;
        } else {
            // 何も操作していない
            if (state_ == FarmerState::Walk || state_ == FarmerState::Run) {
                newState = FarmerState::Idle1;
                idleTimer_ = 0.0f;
            } else if (state_ == FarmerState::Idle1) {
                idleTimer_ += deltaTime;
                if (idleTimer_ >= idleTimeout) {
                    newState = FarmerState::Idle2;
                }
            }
            // Idle2 のまま — 操作があれば上の isMoving や attackTriggered で抜ける
        }

        // --- アニメーション切り替え ---
        if (newState != state_) {
            state_ = newState;
            int targetAnim = GetAnimIndexForState(state_);
            if (targetAnim != currentAnimIndex_) {
                RC::CrossfadeModelAnimation(ren->modelHandle, modelPath_, targetAnim, blendDuration);
                currentAnimIndex_ = targetAnim;
            }
            if (state_ == FarmerState::Attack) {
                // 攻撃: 実際に読み込まれたアニメーションの再生期間を取得してタイマーをセット
                float duration = RC::GetModelAnimationDuration(ren->modelHandle);
                attackTimer_ = (duration > 0.0f) ? duration : 1.0f;
            }
        }

        // 初回フレーム: まだアニメーションが設定されていない場合
        if (currentAnimIndex_ < 0) {
            currentAnimIndex_ = animIdle1;
            RC::AttachModelAnimation(ren->modelHandle, modelPath_, animIdle1);
        }

        // --- 移動処理 ---
        if (state_ != FarmerState::Attack && isMoving) {
            float speed = (state_ == FarmerState::Run) ? runSpeed : walkSpeed;
            tr->position.x += moveX * speed * deltaTime;
            tr->position.z += moveZ * speed * deltaTime;

            // 移動方向に向きを回転（Y軸周り）
            // Y軸回転の向きがワールドX軸と逆手のため、X成分の符号を反転させる
            // （これを入れないと前後は正しく、左右だけ反転して見える）
            float targetAngle = std::atan2(-moveX, moveZ);
            // 角度の最短経路補間
            float diff = targetAngle - facingAngle_;
            while (diff > 3.14159265f) diff -= 6.28318530f;
            while (diff < -3.14159265f) diff += 6.28318530f;
            facingAngle_ += diff * (std::min)(1.0f, rotationSpeed * deltaTime);

            // モデル正面を移動方向（facingAngle_）に向かせる
            tr->rotation.y = facingAngle_;
        }

        // --- カメラ追従 ---
        UpdateCamera(tr, deltaTime);
    }

    void OnDestroy() override {
        std::cout << "[FarmerController] OnDestroy\n";
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::Text("State: %s", GetStateName(state_));
        ImGui::DragFloat("Walk Speed", &walkSpeed, 0.1f, 0.1f, 20.0f);
        ImGui::DragFloat("Run Speed", &runSpeed, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Idle Timeout", &idleTimeout, 0.1f, 1.0f, 30.0f);
        ImGui::DragFloat("Blend Duration", &blendDuration, 0.01f, 0.01f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Animation Indices:");
        ImGui::DragInt("Idle1", &animIdle1, 1, 0, 10);
        ImGui::DragInt("Idle2", &animIdle2, 1, 0, 10);
        ImGui::DragInt("Walk", &animWalk, 1, 0, 10);
        ImGui::DragInt("Run", &animRun, 1, 0, 10);
        ImGui::DragInt("Attack", &animAttack, 1, 0, 10);
        ImGui::Separator();
        ImGui::Text("Camera:");
        ImGui::DragFloat("Camera Distance", &cameraDistance, 0.1f, 1.0f, 30.0f);
        ImGui::DragFloat("Camera Height", &cameraHeight, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Camera Look Height", &cameraLookHeight, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Camera Dead Zone X", &cameraDeadZoneX, 0.1f, 0.0f, 10.0f);
#endif
    }

    nlohmann::json Serialize() override {
        return {
            {"walkSpeed", walkSpeed},
            {"runSpeed", runSpeed},
            {"idleTimeout", idleTimeout},
            {"blendDuration", blendDuration},
            {"animIdle1", animIdle1},
            {"animIdle2", animIdle2},
            {"animWalk", animWalk},
            {"animRun", animRun},
            {"animAttack", animAttack},
            {"cameraDistance", cameraDistance},
            {"cameraHeight", cameraHeight},
            {"cameraLookHeight", cameraLookHeight},
            {"cameraDeadZoneX", cameraDeadZoneX}
        };
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("walkSpeed")) walkSpeed = j["walkSpeed"].get<float>();
        if (j.contains("runSpeed")) runSpeed = j["runSpeed"].get<float>();
        if (j.contains("idleTimeout")) idleTimeout = j["idleTimeout"].get<float>();
        if (j.contains("blendDuration")) blendDuration = j["blendDuration"].get<float>();
        if (j.contains("animIdle1")) animIdle1 = j["animIdle1"].get<int>();
        if (j.contains("animIdle2")) animIdle2 = j["animIdle2"].get<int>();
        if (j.contains("animWalk")) animWalk = j["animWalk"].get<int>();
        if (j.contains("animRun")) animRun = j["animRun"].get<int>();
        if (j.contains("animAttack")) animAttack = j["animAttack"].get<int>();
        if (j.contains("cameraDistance")) cameraDistance = j["cameraDistance"].get<float>();
        if (j.contains("cameraHeight")) cameraHeight = j["cameraHeight"].get<float>();
        if (j.contains("cameraLookHeight")) cameraLookHeight = j["cameraLookHeight"].get<float>();
        if (j.contains("cameraDeadZoneX")) cameraDeadZoneX = j["cameraDeadZoneX"].get<float>();
    }

private:
    FarmerState state_ = FarmerState::Idle1;
    float idleTimer_ = 0.0f;
    float attackTimer_ = 0.0f;
    float facingAngle_ = 0.0f;
    int currentAnimIndex_ = -1;
    bool isCameraInitialized_ = false;
    std::string modelPath_;

    int GetAnimIndexForState(FarmerState s) const {
        switch (s) {
            case FarmerState::Idle1:  return animIdle1;
            case FarmerState::Idle2:  return animIdle2;
            case FarmerState::Walk:   return animWalk;
            case FarmerState::Run:    return animRun;
            case FarmerState::Attack: return animAttack;
            default: return animIdle1;
        }
    }

    static const char* GetStateName(FarmerState s) {
        switch (s) {
            case FarmerState::Idle1:  return "Idle1";
            case FarmerState::Idle2:  return "Idle2";
            case FarmerState::Walk:   return "Walk";
            case FarmerState::Run:    return "Run";
            case FarmerState::Attack: return "Attack";
            default: return "Unknown";
        }
    }

    void UpdateCamera(TransformComponent* playerTr, float deltaTime) {
        Scene* scene = GetScene();
        if (!scene) return;

        // シーン内のメインカメラを検索
        for (auto& e : scene->GetEntities()) {
            auto* camComp = e->GetComponent<CameraComponent>();
            auto* camTr = e->GetComponent<TransformComponent>();
            if (!camComp || !camTr || !camComp->isMain) continue;

            // 俯瞰追従カメラ: プレイヤーの移動に対して最も見やすく酔いにくい仕様に最適化
            // カメラは常にプレイヤーより -Z 側（画面奥に向かって手前）に位置し続けるため、
            // W/Sどちらが前進方向でもこのオフセットの符号は変える必要がない
            float targetCamZ = playerTr->position.z - cameraDistance;
            float targetCamY = playerTr->position.y + cameraHeight;

            if (!isCameraInitialized_) {
                camTr->position.x = playerTr->position.x;
                camTr->position.y = targetCamY;
                camTr->position.z = targetCamZ;
                isCameraInitialized_ = true;
            } else {
                // 【横方向 (X軸)】
                // プレイヤーを中心に一定幅の「デッドゾーン」を設け、その範囲内で左右に
                // 動いてもカメラは一切動かさない（据え置き）。
                // ゾーンの外に出た瞬間からは、今まで通りズレなく瞬間追従させる。
                // ※ここに時間ベースのなめらかな補間（Lerp/Exp平滑化）を入れると、
                // 　X軸の追従が奥行き(Z)方向の追従と異なる速度で減衰するため、
                // 　transform投影の関係で見かけ上「カメラが弧を描いて回り込む」
                // 　（円を描くような挙動・W/Sの前後が反転して見える）錯覚が発生する。
                // 　そのため追従を始めたら必ず瞬間的にズレなく一致させ、
                // 　「静止」と「ズレなしの瞬間追従」の２状態だけにする。
                float offsetX = playerTr->position.x - camTr->position.x;
                if (offsetX > cameraDeadZoneX) {
                    camTr->position.x = playerTr->position.x - cameraDeadZoneX;
                } else if (offsetX < -cameraDeadZoneX) {
                    camTr->position.x = playerTr->position.x + cameraDeadZoneX;
                }
                // デッドゾーン内であればカメラのXは据え置き（移動しない）

                // 【前後方向・高低 (Z軸 / Y軸)】
                // 前進（Wキー）や後退（Sキー）には滑らかなバネ追従（スムージング）を持たせる
                // 酔い防止＆疾走感のため、ダッシュ(Run)時には追従スピードを落として程よく後ろに遅らせる
                float followSpeed = (state_ == FarmerState::Run) ? 4.0f : 10.0f;
                float alpha = 1.0f - std::exp(-followSpeed * deltaTime);

                camTr->position.y += (targetCamY - camTr->position.y) * alpha;
                camTr->position.z += (targetCamZ - camTr->position.z) * alpha;
            }

            // カメラの姿勢（Rotation）
            // 全方位において安定した美しい見下げアングルをキープし、画面のブレ・回転酔いを除去する
            float dy = cameraLookHeight - cameraHeight;
            float dz = cameraDistance;
            float dist = std::sqrt(dz * dz);

            camTr->rotation.x = std::atan2(-dy, dist); // Pitch（一定の見下ろし角度）
            camTr->rotation.y = 0.0f;                  // Yaw（固定）
            camTr->rotation.z = 0.0f;                  // Roll（固定）

            break; // メインカメラは1つだけ
        }
    }
};

REGISTER_SCRIPT(FarmerController)
