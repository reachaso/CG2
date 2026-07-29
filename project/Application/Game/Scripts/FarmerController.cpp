#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/AnimationComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/GPUParticleComponent.h"
#include "Particle/GPUParticle.h"
#include "Input/Input.h"
#include "Common/EngineConfig.h"
#include "Math/Math.h"
#include "RenderCommon.h"
#include "Scene.h"
#include "CharacterTags.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <memory>
#include <string>

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
/// WASD移動、SHIFT+WASDで走り、左クリックで攻撃、Spaceでジャンプ、放置で待機アニメーション切り替え
class FarmerController : public ScriptableEntity {
public:
    // アニメーションインデックス（GLB内の順番）
    int animIdle1  = 4;  ///< 待機1 (通常待機)
    int animIdle2  = 1;  ///< 待機2 (5秒放置後の待機)
    int animWalk   = 0;  ///< 歩き
    int animRun    = 2;  ///< 走り
    int animAttack = 3;  ///< 切り付け

    // 注意: これらはあくまで「新規アタッチ時の初期値」。
    // シーンに保存済みのエンティティは CG4.json の scriptDataList が
    // Deserialize で上書きするため、既存 Farmer の速度を変えたい場合は
    // シーンファイル側（またはインスペクタ → シーン保存）を編集すること。
    float walkSpeed = 1.0f;        ///< 歩き速度 (m/s)
    float runSpeed  = 3.0f;        ///< 走り速度 (m/s)
    float rotationSpeed = 10.0f;   ///< 回転の滑らかさ
    float idleTimeout = 5.0f;      ///< 待機2に切り替わるまでの秒数
    float blendDuration = 0.2f;    ///< アニメーションブレンド時間
    float collisionStep = 0.1f;    ///< 当たり判定の分割ステップ(m)。

    // ゲームパッド設定 (XInput)
    float padDeadZone = 0.24f;         ///< 左スティックのデッドゾーン (0〜1)。XInput 推奨値相当
    float padTriggerThreshold = 0.3f;  ///< RT をダッシュとみなす引き量 (0〜1)

    /// @brief 鍬を装備していないと攻撃できないようにするか
    /// @details 装備状態は HoeItem が Entity タグ "farmerHasHoe" に書き込んでいる
    bool requireHoeToAttack = true;

    // カメラ追従設定
    float cameraDistance = 8.0f;   ///< カメラ距離
    float cameraHeight = 3.0f;     ///< カメラ高さ
    float cameraLookHeight = 1.0f; ///< 注視点の高さオフセット
    float cameraDeadZoneX = 1.5f;      ///< 横方向のデッドゾーン幅（プレイヤーがこの範囲内にいる間はカメラのXを動かさない。範囲外に出たら瞬間的にズレなく追従する）

    // ============================================================
    // ジャンプ（Space キー / パッド A ボタン）
    // ============================================================
    /// @brief ジャンプを使えるようにするか
    bool jumpEnabled = true;
    /// @brief 踏み切り速度 (m/s)。大きいほど高く跳ぶ
    float jumpSpeed = 4.5f;
    /// @brief 落下加速度 (m/s^2)
    float jumpGravity = 12.0f;

    // ============================================================
    // 手の炎（右クリック / パッド Y ボタンの長押しで点火）
    // ============================================================
    /// @brief 手の炎を使えるようにするか
    bool handFireEnabled = true;
    /// @brief 点火までの長押し時間 (秒)
    /// @details この時間押し続けたら点火し、離した瞬間に消える。
    float fireHoldTime = 0.5f;
    /// @brief 炎を出す手のジョイント名
    std::string fireHandJoint = "R_Hand";
    /// @brief 手のジョイントから炎の中心までのオフセット (手のローカル基準, m)
    /// @details farmer.glb の R_Hand は手首にあり指のボーンが無いため、
    ///          HoeItem の握り位置と同じ考え方で手のひら付近まで進める。
    ///          このリグはボーンが親のローカル +Y 方向へ伸びるので +Y が指先方向。
    RC::Vector3 fireHandSocketOffset = {0.0f, 0.10f, 0.0f};
    /// @brief 炎全体の大きさ倍率
    float fireSize = 1.0f;
    /// @brief 1フレームあたりの射出数（多いほど密で明るい炎になる）
    int fireEmitCount = 14;

protected:
    void OnCreate() override {
        std::cout << "[FarmerController] OnCreate\n";
        state_ = FarmerState::Idle1;
        idleTimer_ = 0.0f;
        attackTimer_ = 0.0f;
        facingAngle_ = 0.0f;
        currentAnimIndex_ = -1; // 未設定
        isCameraInitialized_ = false;
        fireHoldTimer_ = 0.0f;
        fireActive_ = false;
        warnedFireJoint_ = false;
        fireEmitter_.reset();
        isJumping_ = false;
        verticalVelocity_ = 0.0f;
        if (auto* tr = GetComponent<TransformComponent>()) {
            // 初期向き: 画面奥（+Z / Wキーの前進方向）を向かせる
            tr->rotation.y = facingAngle_;
            groundY_ = tr->position.y; // 着地面の高さ（ジャンプの基準）
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

        // --- 入力を受け付けてよい状況かの判定 ---
        // Input は DirectInput から直接状態を取るため、エディタのUI上をクリックしても
        // 区別なくゲーム入力として飛んでくる。そのままだと再生ボタンを押した瞬間の
        // 左クリックで攻撃が暴発するので、ここでフィルタする。
        const bool acceptInput = IsScenePlaying();               // 停止中/一時停止中は入力を無視
        const bool acceptMouse = acceptInput && IsMouseForGame(input);   // ビューポート内のみ
        const bool acceptKeys  = acceptInput && IsKeyboardForGame();     // 文字入力中は無視
        const bool acceptPad   = acceptInput;                            // パッドはカーソル位置に依存しない
        // 攻撃は鍬（武器）を持っているときだけ受け付ける
        hasHoe_ = HasHoeEquipped();
        const bool acceptAttack = acceptInput && (!requireHoeToAttack || hasHoe_);

        // --- 入力の取得 ---
        // 操作方法:
        //   移動   WASD          / 左スティック
        //   ダッシュ Shift        / RT
        //   攻撃   左クリック     / Xボタン
        //   手の炎 右クリック長押し / Yボタン長押し
        //   ジャンプ Space         / Aボタン
        float moveX = 0.0f;
        float moveZ = 0.0f;
        bool shiftHeld = false;
        bool attackTriggered = false;
        bool fireHeld = false;   ///< 手の炎のボタンを「押し続けている」か
        bool jumpTriggered = false; ///< このフレームでジャンプボタンが押された瞬間か

        // --- キーボード / マウス ---
        // W=前進（カメラから離れる＝画面奥方向）／S=後退（カメラに近づく＝画面手前方向）
        // カメラはプレイヤーの -Z 側に位置し Yaw=0（+Z を向く）ため、
        // 画面奥 = ワールド +Z、画面手前 = ワールド -Z となる
        if (acceptKeys) {
            if (input->IsKeyPressed(DIK_W)) moveZ += 1.0f;
            if (input->IsKeyPressed(DIK_S)) moveZ -= 1.0f;
            if (input->IsKeyPressed(DIK_D)) moveX += 1.0f;
            if (input->IsKeyPressed(DIK_A)) moveX -= 1.0f;

            shiftHeld = input->IsKeyPressed(DIK_LSHIFT) || input->IsKeyPressed(DIK_RSHIFT);

            // Space: ジャンプ（押した瞬間のみ。押し続けても連続ジャンプしない）
            if (input->IsKeyTrigger(DIK_SPACE)) jumpTriggered = true;
        }
        if (acceptAttack && acceptMouse && input->IsMouseTrigger(0)) attackTriggered = true;

        // 右クリック（ボタン番号1）を押し続けている間 true。
        // 攻撃と違い Trigger ではなく Pressed を見るのは、長押し時間を測るため。
        if (acceptMouse && input->IsMousePressed(1)) fireHeld = true;

        // --- ゲームパッド (XInput) ---
        padConnected_ = input->IsXInputConnected();
        if (acceptPad && padConnected_) {
            // 左スティック: 移動（傾き量がそのまま速度になる＝微速歩行が可能）
            float padX = static_cast<float>(input->GetXInputThumbLX()) / 32767.0f;
            float padZ = static_cast<float>(input->GetXInputThumbLY()) / 32767.0f; // 上 = 画面奥(+Z)
            ApplyRadialDeadZone(padX, padZ, padDeadZone);
            moveX += padX;
            moveZ += padZ;

            // RT: ダッシュ（0〜255 を 0〜1 に正規化して閾値と比較）
            const float rt = static_cast<float>(input->GetXInputRightTrigger()) / 255.0f;
            if (rt >= padTriggerThreshold) shiftHeld = true;

            // Xボタン: 攻撃（押した瞬間のみ）
            if (acceptAttack && input->IsXInputButtonTrigger(XINPUT_GAMEPAD_X)) attackTriggered = true;

            // Yボタン: 手の炎（押し続けている間 true）
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_Y)) fireHeld = true;

            // Aボタン: ジャンプ（押した瞬間のみ）
            if (input->IsXInputButtonTrigger(XINPUT_GAMEPAD_A)) jumpTriggered = true;
        }

        // --- 手の炎の長押し判定 ---
        // 入力を受け付けない状況（停止中など）では必ず消火する。
        // fireHeld を溜め込んだまま停止すると、再生した瞬間に点火してしまう。
        UpdateFireHold(acceptInput && fireHeld, deltaTime);

        // キーボードとスティックを同時に入れた場合に長さが1を超えることがあるためクランプ
        moveX = (std::max)(-1.0f, (std::min)(1.0f, moveX));
        moveZ = (std::max)(-1.0f, (std::min)(1.0f, moveZ));

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

        // --- 状態を他スクリプトへ公開 ---
        // FarmerState はこの翻訳単位内のローカル定義なので、他スクリプト（HoeItem など）
        // からは参照できない。Entity のタグ経由で数値として公開しておく。
        // "farmerMoving": 1 なら Walk または Run 中（装備アイテムの表示切り替えに使う）
        if (Entity* self = GetEntity()) {
            const bool moving = (state_ == FarmerState::Walk || state_ == FarmerState::Run);
            self->SetTag(kTagFarmerMoving, moving ? 1 : 0);
            self->SetTag(kTagFarmerAttacking, (state_ == FarmerState::Attack) ? 1 : 0);
            // "farmerFire": 1 なら手に炎が出ている（HoeItem が鍬の描画を消すのに使う）
            self->SetTag(kTagFarmerFire, fireActive_ ? 1 : 0);
        }

        // --- 移動処理 ---
        if (state_ != FarmerState::Attack && isMoving) {
            float speed = (state_ == FarmerState::Run) ? runSpeed : walkSpeed;

            // position を直接足すと、ダッシュ時や deltaTime が跳ねたフレームで
            // 1フレームの移動量が壁の厚みを超え、壁を貫通（トンネリング）してしまう。
            // MoveAndSlide は移動量を collisionStep 以下に分割しながら当たり判定を行うため
            // 高速移動でも壁を飛び越えず、斜めに当たった場合は壁に沿ってスライドする。
            RC::Vector3 delta = {
                moveX * speed * deltaTime,
                0.0f,
                moveZ * speed * deltaTime
            };
            MoveAndSlide(delta, collisionStep);

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

        // --- ジャンプ処理 ---
        // 水平移動のあとに呼ぶ。MoveAndSlide が Y を触らないので Y は自前で積む。
        UpdateJump(tr, acceptInput && jumpTriggered, deltaTime);

        // --- 手の炎のエミッタ追従 ---
        // 移動と回転を反映したあとに呼ぶこと。先に呼ぶと炎だけ1フレーム遅れる。
        UpdateHandFire(ren, tr);

        // --- カメラ追従 ---
        UpdateCamera(tr, deltaTime);
    }

    void OnDestroy() override {
        std::cout << "[FarmerController] OnDestroy\n";

        // 炎を出したまま農民が消えると、エミッタだけがシーンに残って
        // 最後の手の位置で炎を吹き続けてしまう。一緒に片付ける。
        if (auto emitter = fireEmitter_.lock()) {
            if (auto* gpu = emitter->GetComponent<GPUParticleComponent>()) {
                if (gpu->particleSystem) gpu->particleSystem->SetEmitCount(0);
            }
            emitter->Destroy();
        }
        fireEmitter_.reset();
        fireActive_ = false;
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::Text("State: %s", GetStateName(state_));
        ImGui::DragFloat("Walk Speed", &walkSpeed, 0.1f, 0.1f, 20.0f);
        ImGui::DragFloat("Run Speed", &runSpeed, 0.1f, 0.1f, 30.0f);
        ImGui::DragFloat("Idle Timeout", &idleTimeout, 0.1f, 1.0f, 30.0f);
        ImGui::DragFloat("Blend Duration", &blendDuration, 0.01f, 0.01f, 1.0f);
        ImGui::DragFloat("Collision Step (m)", &collisionStep, 0.01f, 0.01f, 1.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("当たり判定の分割幅。最も薄い壁の厚みより小さくすること");
        }
        ImGui::Separator();
        ImGui::Text("操作: 移動 WASD / 左スティック");
        ImGui::Text("      ダッシュ Shift / RT");
        ImGui::Text("      攻撃 左クリック / Xボタン");
        ImGui::Text("      手の炎 右クリック長押し / Yボタン長押し");
        ImGui::Text("      ジャンプ Space / Aボタン");
        ImGui::Text("Gamepad: %s", padConnected_ ? "接続中" : "未接続");
        ImGui::DragFloat("Pad Dead Zone", &padDeadZone, 0.01f, 0.0f, 0.95f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("左スティックの遊び。大きくすると微速歩行しづらくなる");
        }
        ImGui::DragFloat("Pad RT Threshold", &padTriggerThreshold, 0.01f, 0.01f, 1.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("RT をこの割合まで引いたらダッシュ扱いになる");
        }
        ImGui::Separator();
        ImGui::Text("鍬: %s", hasHoe_ ? "装備中" : "未装備");
        ImGui::Checkbox("鍬がないと攻撃できない", &requireHoeToAttack);
        ImGui::Separator();
        ImGui::Text("Animation Indices:");
        ImGui::DragInt("Idle1", &animIdle1, 1, 0, 10);
        ImGui::DragInt("Idle2", &animIdle2, 1, 0, 10);
        ImGui::DragInt("Walk", &animWalk, 1, 0, 10);
        ImGui::DragInt("Run", &animRun, 1, 0, 10);
        ImGui::DragInt("Attack", &animAttack, 1, 0, 10);
        ImGui::Separator();
        ImGui::Text("ジャンプ (Space / Aボタン):");
        ImGui::Checkbox("ジャンプを有効にする", &jumpEnabled);
        ImGui::Text("状態: %s", isJumping_ ? "空中" : "接地");
        ImGui::DragFloat("踏み切り速度 (m/s)", &jumpSpeed, 0.1f, 0.5f, 20.0f);
        ImGui::DragFloat("落下加速度 (m/s^2)", &jumpGravity, 0.5f, 1.0f, 60.0f);
        ImGui::Separator();
        ImGui::Text("手の炎 (右クリック長押し / Yボタン長押し):");
        ImGui::Checkbox("手の炎を有効にする", &handFireEnabled);
        ImGui::Text("状態: %s", fireActive_ ? "点火中" : "消火");
        ImGui::ProgressBar((fireHoldTime > 0.0f) ? (fireHoldTimer_ / fireHoldTime) : 0.0f,
                           ImVec2(0.0f, 0.0f), "長押し");
        if (!fireJointFound_ && fireActive_) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                               "ジョイント '%s' が見つかりません", fireHandJoint.c_str());
        }
        ImGui::DragFloat("点火までの長押し (秒)", &fireHoldTime, 0.05f, 0.0f, 3.0f);
        ImGui::DragFloat("炎の大きさ", &fireSize, 0.05f, 0.1f, 5.0f);
        ImGui::DragInt("1フレームの射出数", &fireEmitCount, 1, 1, 100);
        ImGui::DragFloat3("炎の位置 (手のローカル)", &fireHandSocketOffset.x, 0.005f, -0.5f, 0.5f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("R_Hand は手首にあるため、Y を増やすと手のひら側へ移動する");
        }
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
            {"collisionStep", collisionStep},
            {"padDeadZone", padDeadZone},
            {"padTriggerThreshold", padTriggerThreshold},
            {"requireHoeToAttack", requireHoeToAttack},
            {"animIdle1", animIdle1},
            {"animIdle2", animIdle2},
            {"animWalk", animWalk},
            {"animRun", animRun},
            {"animAttack", animAttack},
            {"cameraDistance", cameraDistance},
            {"cameraHeight", cameraHeight},
            {"cameraLookHeight", cameraLookHeight},
            {"cameraDeadZoneX", cameraDeadZoneX},
            {"jumpEnabled", jumpEnabled},
            {"jumpSpeed", jumpSpeed},
            {"jumpGravity", jumpGravity},
            {"handFireEnabled", handFireEnabled},
            {"fireHoldTime", fireHoldTime},
            {"fireHandJoint", fireHandJoint},
            {"fireHandSocketOffset", {fireHandSocketOffset.x, fireHandSocketOffset.y, fireHandSocketOffset.z}},
            {"fireSize", fireSize},
            {"fireEmitCount", fireEmitCount}
        };
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("walkSpeed")) walkSpeed = j["walkSpeed"].get<float>();
        if (j.contains("runSpeed")) runSpeed = j["runSpeed"].get<float>();
        if (j.contains("idleTimeout")) idleTimeout = j["idleTimeout"].get<float>();
        if (j.contains("blendDuration")) blendDuration = j["blendDuration"].get<float>();
        if (j.contains("collisionStep")) collisionStep = j["collisionStep"].get<float>();
        if (j.contains("padDeadZone")) padDeadZone = j["padDeadZone"].get<float>();
        if (j.contains("padTriggerThreshold")) padTriggerThreshold = j["padTriggerThreshold"].get<float>();
        if (j.contains("requireHoeToAttack")) requireHoeToAttack = j["requireHoeToAttack"].get<bool>();
        if (j.contains("animIdle1")) animIdle1 = j["animIdle1"].get<int>();
        if (j.contains("animIdle2")) animIdle2 = j["animIdle2"].get<int>();
        if (j.contains("animWalk")) animWalk = j["animWalk"].get<int>();
        if (j.contains("animRun")) animRun = j["animRun"].get<int>();
        if (j.contains("animAttack")) animAttack = j["animAttack"].get<int>();
        if (j.contains("cameraDistance")) cameraDistance = j["cameraDistance"].get<float>();
        if (j.contains("cameraHeight")) cameraHeight = j["cameraHeight"].get<float>();
        if (j.contains("cameraLookHeight")) cameraLookHeight = j["cameraLookHeight"].get<float>();
        if (j.contains("cameraDeadZoneX")) cameraDeadZoneX = j["cameraDeadZoneX"].get<float>();
        if (j.contains("jumpEnabled")) jumpEnabled = j["jumpEnabled"].get<bool>();
        if (j.contains("jumpSpeed")) jumpSpeed = j["jumpSpeed"].get<float>();
        if (j.contains("jumpGravity")) jumpGravity = j["jumpGravity"].get<float>();
        if (j.contains("handFireEnabled")) handFireEnabled = j["handFireEnabled"].get<bool>();
        if (j.contains("fireHoldTime")) fireHoldTime = j["fireHoldTime"].get<float>();
        if (j.contains("fireHandJoint")) fireHandJoint = j["fireHandJoint"].get<std::string>();
        if (j.contains("fireHandSocketOffset")) {
            auto& v = j["fireHandSocketOffset"];
            fireHandSocketOffset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
        }
        if (j.contains("fireSize")) fireSize = j["fireSize"].get<float>();
        if (j.contains("fireEmitCount")) fireEmitCount = j["fireEmitCount"].get<int>();
    }

private:
    FarmerState state_ = FarmerState::Idle1;
    float idleTimer_ = 0.0f;
    float attackTimer_ = 0.0f;
    float facingAngle_ = 0.0f;
    int currentAnimIndex_ = -1;
    bool isCameraInitialized_ = false;
    std::string modelPath_;

    bool padConnected_ = false; ///< 直近のフレームでゲームパッドが接続されていたか（ImGui表示用）
    bool hasHoe_ = false;       ///< 直近のフレームで鍬を装備していたか（ImGui表示用）

    // --- ジャンプ ---
    bool isJumping_ = false;         ///< 空中にいるか
    float verticalVelocity_ = 0.0f;  ///< 上方向の速度 (m/s)
    float groundY_ = 0.0f;           ///< 踏み切った地面の高さ（着地判定の基準）

    // --- 手の炎 ---
    float fireHoldTimer_ = 0.0f;   ///< ボタンを押し続けている時間 (秒)
    bool fireActive_ = false;      ///< 点火中か
    bool fireJointFound_ = false;  ///< 直近のフレームで手のジョイントを取得できたか
    bool warnedFireJoint_ = false; ///< ジョイント名の警告を出したか（毎フレーム出さない）
    /// @brief 炎のエミッタエンティティ
    /// @details 生ポインタではなく weak_ptr で持つ。シーンから取り除かれたら
    ///          expired になるので、消えたエンティティを触ってしまう事故を防げる。
    std::weak_ptr<Entity> fireEmitter_;

    /// @brief 鍬を装備しているか
    /// @details HoeItem が入手時に Entity タグへ書き込む。スクリプト同士は直接参照できないため
    ///          CharacterTags.h のキーを介してやり取りする。
    bool HasHoeEquipped() const {
        const Entity* self = GetEntity();
        return self && self->GetTagInt(kTagFarmerHasHoe, 0) != 0;
    }

    // ============================================================
    // 入力ヘルパー
    // ============================================================

    /// @brief スティック入力に円形デッドゾーンを適用する
    /// @param x スティックX (-1〜1)。補正後の値が書き戻される
    /// @param y スティックY (-1〜1)。補正後の値が書き戻される
    /// @param deadZone デッドゾーン半径 (0〜1)
    /// @details 軸ごとに閾値を切るのではなく「傾き量（ベクトルの長さ）」で判定し、
    ///          デッドゾーンを超えた分を 0〜1 に再スケールする。
    ///          軸ごとの判定だと斜め入力で実効デッドゾーンが変わり、
    ///          再スケールしないと閾値を超えた瞬間に速度が飛ぶ（微速歩行ができない）。
    static void ApplyRadialDeadZone(float& x, float& y, float deadZone) {
        const float lenSq = x * x + y * y;
        if (lenSq <= 0.0f) { x = 0.0f; y = 0.0f; return; }

        const float len = std::sqrt(lenSq);
        const float dz = (std::max)(0.0f, (std::min)(0.95f, deadZone));
        if (len <= dz) { x = 0.0f; y = 0.0f; return; }

        // 傾き量を 0〜1 に伸ばし直す（len は 1 を少し超えることがあるのでクランプ）
        const float scaled = (std::min)(1.0f, (len - dz) / (1.0f - dz));
        x = (x / len) * scaled;
        y = (y / len) * scaled;
    }

    // ============================================================
    // 入力フィルタ（エディタ上の誤操作でゲームが反応しないようにする）
    // ============================================================

    /// @brief シーンが再生中か
    /// @details 停止中・一時停止中は deltaTime が 0 で状態が進まないため、
    ///          クリックで Attack に入るとタイマーが減らず抜け出せなくなる。
    ///          そもそも入力を受け付けないようにしておく。
    /// @note SceneContext が無い場合（単体テスト等）は true 扱い
    bool IsScenePlaying() const {
        SceneContext* ctx = GetSceneContext();
        return ctx ? ctx->isPlaying() : true;
    }

    /// @brief マウス入力をゲーム操作として扱ってよいか
    /// @details エディタのビューポート（プレビュー）内にカーソルがあるときのみ true。
    ///          再生ボタン・インスペクタ・ヒエラルキー上のクリックを弾く。
    ///          ホバー状態は EditorManager が毎フレーム Input へ伝えている。
    static bool IsMouseForGame(Input* input) {
#if RC_ENABLE_IMGUI
        return input && input->IsViewportHovered();
#else
        // エディタを含まないビルドではウィンドウ全体がゲーム画面
        (void)input;
        return true;
#endif
    }

    /// @brief キーボード入力をゲーム操作として扱ってよいか
    /// @details インスペクタで名前を入力している最中などに WASD で動かないようにする
    static bool IsKeyboardForGame() {
#if RC_ENABLE_IMGUI
        return !ImGui::GetIO().WantCaptureKeyboard;
#else
        return true;
#endif
    }

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

    // ============================================================
    // ジャンプ
    // ============================================================

    /// @brief ジャンプの踏み切りと落下を処理する
    /// @param tr 自分の Transform
    /// @param triggered このフレームでジャンプボタンが押された瞬間か
    /// @param deltaTime フレーム間隔 (秒)
    /// @details 地面の高さは「踏み切った瞬間の Y」を基準にする。
    ///          接地中は毎フレーム更新するので、坂や台の上に立っていても
    ///          その高さへ着地する。空中では二段ジャンプさせない。
    void UpdateJump(TransformComponent* tr, bool triggered, float deltaTime) {
        if (!tr) return;

        if (!jumpEnabled) {
            // 途中で無効化された場合に空中で固まらないよう地面へ戻す
            if (isJumping_) {
                tr->position.y = groundY_;
                isJumping_ = false;
                verticalVelocity_ = 0.0f;
            }
            return;
        }

        if (!isJumping_) {
            // 接地中: 立っている高さを基準として覚えておく
            groundY_ = tr->position.y;

            // 攻撃モーション中は踏み切らせない（アニメーションが破綻する）
            if (triggered && state_ != FarmerState::Attack) {
                isJumping_ = true;
                verticalVelocity_ = jumpSpeed;
            }
            return;
        }

        // 空中: 重力で速度を減らしながら Y を積む
        verticalVelocity_ -= jumpGravity * deltaTime;
        tr->position.y += verticalVelocity_ * deltaTime;

        // 着地
        if (tr->position.y <= groundY_) {
            tr->position.y = groundY_;
            isJumping_ = false;
            verticalVelocity_ = 0.0f;
        }
    }

    // ============================================================
    // 手の炎
    // ============================================================

    /// @brief 長押しの経過時間から点火状態を決める
    /// @param held このフレームでボタンを押し続けているか
    /// @param deltaTime フレーム間隔 (秒)
    /// @details 「長押しで点火し、離すまで継続」なので、
    ///          点火後もタイマーは減らさず、離した瞬間に 0 に戻す。
    void UpdateFireHold(bool held, float deltaTime) {
        if (!handFireEnabled || !held) {
            fireHoldTimer_ = 0.0f;
            fireActive_ = false;
            return;
        }

        fireHoldTimer_ += deltaTime;
        if (fireHoldTimer_ >= fireHoldTime) {
            fireActive_ = true;
        }
    }

    /// @brief 炎のエミッタを手のジョイントに追従させる
    /// @param ren 農民の ModelRendererComponent（ジョイント行列の取得元）
    /// @param tr 農民の TransformComponent
    /// @details エミッタは点火のたびに作り直さず、一度作ったら使い回して
    ///          射出数 0/N で点火・消火を切り替える。
    ///          毎回 Destroy すると、まだ残っている炎の粒が消えてしまうし、
    ///          エンティティの生成・破棄をボタン連打の回数だけ繰り返すことになる。
    void UpdateHandFire(ModelRendererComponent* ren, TransformComponent* tr) {
        // まだ一度も点火していないならエミッタを作らない（無駄なエンティティを生やさない）
        if (!fireActive_ && fireEmitter_.expired()) return;

        auto emitter = EnsureFireEmitter();
        if (!emitter) return;

        auto* gpu = emitter->GetComponent<GPUParticleComponent>();
        if (!gpu || !gpu->particleSystem) return;
        GPUParticle& ps = *gpu->particleSystem;

        if (!fireActive_) {
            // 消火。すでに出ている粒は寿命まで自然に消えるので射出だけ止める
            ps.SetEmitCount(0);
            fireJointFound_ = false; // ImGui 表示が古い値を出し続けないようにする
            return;
        }

        // --- 手のワールド位置を求める ---
        RC::Matrix4x4 jointM{};
        fireJointFound_ = ren->HasModel() &&
            RC::GetModelJointMatrix(ren->modelHandle, fireHandJoint, jointM);
        if (!fireJointFound_) {
            if (!warnedFireJoint_) {
                warnedFireJoint_ = true;
                std::cout << "[FarmerController] 炎のジョイント '" << fireHandJoint
                          << "' が見つかりません。利用可能なジョイント:\n";
                if (ren->HasModel()) {
                    for (const std::string& n : RC::GetModelJointNames(ren->modelHandle)) {
                        std::cout << "    " << n << "\n";
                    }
                }
            }
            ps.SetEmitCount(0);
            return;
        }

        // 手のワールド姿勢 = ジョイント × 農民のワールド
        // （この engine の Multiply(m1, m2) は m1 を先に適用する行ベクトル規約）
        // オフセットは手のローカル座標系で足すので、手が回れば炎の位置も一緒に回る。
        const RC::Matrix4x4 handWorld = Multiply(jointM, tr->GetWorldMatrix());
        const RC::Matrix4x4 socketWorld =
            Multiply(MakeTranslateMatrix(fireHandSocketOffset), handWorld);
        const RC::Vector3 firePos = {
            socketWorld.m[3][0], socketWorld.m[3][1], socketWorld.m[3][2]
        };

        // エミッタ位置は TransformComponent 経由で渡す。
        // DataDrivenScene が毎フレーム emitterPosition_ = tr->position を行うため、
        // ここで直接 emitterPosition_ に入れても上書きされるので意味がない。
        //
        // なお DataDrivenScene の GPUParticle 更新はスクリプト更新より前に走るので、
        // 炎の発生位置は常に1フレーム前の手の位置になる。60FPS なら数mmのズレで、
        // 見た目には分からない。直したい場合はエンジン側の更新順を
        // 「スクリプト → GPUParticle」に変える必要がある。
        if (auto* emitterTr = emitter->GetComponent<TransformComponent>()) {
            emitterTr->position = firePos;
        }

        // インスペクタでの調整を毎フレーム反映させる（大きさ・射出数）
        ApplyFireSize(ps);
        ps.SetEmitCount(static_cast<uint32_t>((std::max)(0, fireEmitCount)));
    }

    /// @brief 炎のエミッタエンティティ名（農民ごとに別名にする）
    std::string FireEmitterName() const {
        const Entity* self = GetEntity();
        return (self ? self->GetName() : std::string("Farmer")) + "_HandFire";
    }

    /// @brief 炎のエミッタを取得する（無ければ作る）
    std::shared_ptr<Entity> EnsureFireEmitter() {
        if (auto cached = fireEmitter_.lock()) return cached;

        Scene* scene = GetScene();
        if (!scene) return nullptr;

        // エミッタは実行時に生やすものだが、点火中にシーンを保存すると
        // CG4.json に書き出されてしまう。その状態で読み込むと既に居るので、
        // 名前で探して使い回す（探さないと起動ごとにエミッタが増えていく）。
        const std::string name = FireEmitterName();
        for (const auto& e : scene->GetEntities()) {
            if (e && e->GetName() == name) {
                fireEmitter_ = e;
                // 保存済みエンティティは GPUParticleComponent の既定値
                // （Default タイプ・射出数10・白い粒）で復元されるため、
                // 設定し直さないと読み込んだ瞬間から白い粒を吹き続ける。
                if (auto* gpu = e->GetComponent<GPUParticleComponent>()) {
                    if (gpu->particleSystem) ConfigureFire(*gpu->particleSystem);
                }
                return e;
            }
        }

        auto emitter = scene->CreateEntity(name);
        if (!emitter) return nullptr;

        emitter->AddComponent<TransformComponent>();
        auto* gpu = &emitter->AddComponent<GPUParticleComponent>();
        if (gpu->particleSystem) {
            // Initialize() より前に設定する。
            // 最大数はバッファサイズの計算に使われるので、
            // 後から変えるとバッファの作り直しが走る。
            ConfigureFire(*gpu->particleSystem);
        }

        // GPUParticle の GPU リソースを確保する（これを忘れると描画されない）
        scene->InitDynamicEntityRuntime(*emitter);

        fireEmitter_ = emitter;
        std::cout << "[FarmerController] 手の炎エミッタを生成: " << name << "\n";
        return emitter;
    }

    /// @brief 炎パーティクルの見た目を設定する
    void ConfigureFire(GPUParticle& ps) const {
        ps.SetMaxParticles(512);
        ps.SetParticleType(ParticleType::Fire);
        // 加算合成。粒が重なった中心が白熱して炎の芯に見える
        ps.SetBlendMode(kBlendModeAdd);
        ps.SetPipelinePrefix("gpu_particle_fire");

        // 寿命は短く。手のひらの炎なので長いと煙のように伸びてしまう。
        // 「初速 × 寿命」がだいたい炎の高さになる（0.6m/s × 0.35s ≒ 20cm）。
        ps.minLifeTime_ = 0.22f;
        ps.maxLifeTime_ = 0.42f;

        // 浮力（負の値で上向きに加速する）。velocity は1フレームあたりの
        // 移動量なので、この程度の小さな値で十分に立ち上がる
        ps.gravity_ = -0.04f;

        // 上向きの初速。0.010 × 60fps = 0.6 m/s
        ps.baseVelocity_ = {0.0f, 0.010f, 0.0f};
        ps.velocityVariance_ = 0.006f;

        // 炎の色: 根元のオレンジ → 先端の赤へ遷移して消える
        ps.startColor_ = {1.0f, 0.68f, 0.20f, 1.0f};
        ps.endColor_   = {0.86f, 0.13f, 0.02f, 0.0f};

        // EmitFire は形状を常に円盤として扱うので emitterShape_ は使わないが、
        // エディタで見たときに誤解しないよう Point にしておく
        ps.emitterShape_ = EmitterShape::Point;

        ps.SetEmitCount(0); // 点火するまで出さない
        ApplyFireSize(ps);
    }

    /// @brief fireSize に応じた大きさ関連のパラメータを反映する
    void ApplyFireSize(GPUParticle& ps) const {
        const float size = (std::max)(0.01f, fireSize);
        ps.minScale_ = 0.09f * size;
        ps.maxScale_ = 0.20f * size;
        // 火種の半径。UpdateFire ではゆらぎの振幅もこの値に比例する
        ps.shapeRadius_ = 0.045f * size;
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
