#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "Input/Input.h"
#include "imgui/imgui.h"
#include <iostream>

/// @brief プレイヤー移動用スクリプト
/// WASD、矢印キー、またはコントローラーで移動を行う
class MoveCollider : public ScriptableEntity {
public:
    float speed = 5.0f; // 移動スピード

protected:
    void OnCreate() override {
        std::cout << "[MoveCollider] OnCreate: アタッチされました！\n";
    }

    void OnUpdate(float deltaTime) override {
        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        Input* input = Input::GetInstance();
        if (!input) return;

        float moveX = 0.0f;
        float moveZ = 0.0f;

        // --- キーボード入力 (WASD / 矢印) ---
        if (input->IsKeyPressed(DIK_W) || input->IsKeyPressed(DIK_UP)) {
            moveZ += 1.0f;
        }
        if (input->IsKeyPressed(DIK_S) || input->IsKeyPressed(DIK_DOWN)) {
            moveZ -= 1.0f;
        }
        if (input->IsKeyPressed(DIK_D) || input->IsKeyPressed(DIK_RIGHT)) {
            moveX += 1.0f;
        }
        if (input->IsKeyPressed(DIK_A) || input->IsKeyPressed(DIK_LEFT)) {
            moveX -= 1.0f;
        }

        // --- コントローラー入力 ---
        if (input->IsXInputConnected()) {
            SHORT thumbX = input->GetXInputThumbLX();
            SHORT thumbY = input->GetXInputThumbLY();
            
            // デッドゾーンを設けて正規化
            const float deadZone = 8000.0f;
            if (abs(thumbX) > deadZone) {
                moveX += static_cast<float>(thumbX) / 32767.0f;
            }
            if (abs(thumbY) > deadZone) {
                moveZ += static_cast<float>(thumbY) / 32767.0f; // Y軸スティックでZ移動
            }

            // D-Pad
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_UP)) {
                moveZ += 1.0f;
            }
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN)) {
                moveZ -= 1.0f;
            }
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_RIGHT)) {
                moveX += 1.0f;
            }
            if (input->IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_LEFT)) {
                moveX -= 1.0f;
            }
        }

        // 複数入力の重複などで1.0を超えないようクランプ
        if (moveX > 1.0f) moveX = 1.0f;
        if (moveX < -1.0f) moveX = -1.0f;
        if (moveZ > 1.0f) moveZ = 1.0f;
        if (moveZ < -1.0f) moveZ = -1.0f;

        // 移動適用
        tr->position.x += moveX * speed * deltaTime;
        tr->position.z += moveZ * speed * deltaTime;
    }

    void OnDestroy() override {
        std::cout << "[MoveCollider] OnDestroy: 破棄されました！\n";
    }

public:
    void OnImGui() override {
        // エディタ（インスペクタ）上で速度を調整できるようにする
        ImGui::DragFloat("Move Speed (移動速度)##MoveCollider", &speed, 0.1f, 0.1f, 100.0f);
    }
};

REGISTER_SCRIPT(MoveCollider)
