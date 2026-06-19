#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "Common/Math/MathUtils.h"
#include <vector>
#include <iostream>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief レール移動（パス移動）を制御するスクリプトコンポーネント
class RailMovementScript : public ScriptableEntity {
public:
    float speed = 5.0f;
    std::vector<RC::Vector3> waypoints;
    int currentWaypointIndex = 0;
    bool isMoving = true;
    bool loop = false;

protected:
    void OnCreate() override {
        // 初期化時に何もウェイポイントがなければ、現在位置を追加しておく
        if (waypoints.empty()) {
            if (auto* tr = GetComponent<TransformComponent>()) {
                waypoints.push_back(tr->position);
            }
        }
    }

    void OnUpdate(float deltaTime) override {
        if (!isMoving || waypoints.empty() || currentWaypointIndex >= waypoints.size()) {
            return;
        }

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        RC::Vector3 targetPos = waypoints[currentWaypointIndex];
        RC::Vector3 diff = RC::Sub(targetPos, tr->position);
        float dist = RC::Length(diff);

        float moveAmount = speed * deltaTime;

        if (dist <= moveAmount) {
            // ターゲット地点に到達した
            tr->position = targetPos;
            currentWaypointIndex++;
            if (currentWaypointIndex >= waypoints.size()) {
                if (loop) {
                    currentWaypointIndex = 0; // ループする場合
                } else {
                    isMoving = false; // 終点に到達して停止
                }
            }
        } else {
            // まだ到達していないので進む
            RC::Vector3 dir = RC::SafeNormalize(diff);
            tr->position = RC::Add(tr->position, RC::Mul(dir, moveAmount));
            
            // TODO: カメラやプレイヤーの向き（Rotation）を進行方向に向ける処理を将来的に追加する
        }
    }

    void OnDestroy() override {
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::Checkbox("Is Moving", &isMoving);
        ImGui::Checkbox("Loop", &loop);
        ImGui::DragFloat("Speed", &speed, 0.1f, 0.1f, 100.0f);
        
        ImGui::Text("Waypoints (Count: %zu)", waypoints.size());
        ImGui::Text("Current Index: %d", currentWaypointIndex);
        
        if (ImGui::Button("Add Waypoint")) {
            if (!waypoints.empty()) {
                // 最後の要素をコピーして追加
                waypoints.push_back(waypoints.back());
            } else if (auto* tr = GetComponent<TransformComponent>()) {
                // Transformの現在位置を追加
                waypoints.push_back(tr->position);
            } else {
                waypoints.push_back({0,0,0});
            }
        }

        ImGui::Separator();
        for (size_t i = 0; i < waypoints.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("WP %zu", i);
            ImGui::SameLine();
            ImGui::DragFloat3("##pos", &waypoints[i].x, 0.1f);
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                waypoints.erase(waypoints.begin() + i);
                // インデックスの範囲外アクセスを防ぐ
                if (currentWaypointIndex >= waypoints.size()) {
                    currentWaypointIndex = static_cast<int>(waypoints.size()) - 1;
                    if (currentWaypointIndex < 0) currentWaypointIndex = 0;
                }
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
#endif
    }
};

REGISTER_SCRIPT(RailMovementScript)
