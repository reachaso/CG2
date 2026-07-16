#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "Common/Math/MathUtils.h"
#include "RenderCommon.h"
#include <vector>
#include <iostream>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

struct Waypoint {
    RC::Vector3 pos;
    float waitTime = 0.0f;
};

/// @brief レール移動（パス移動）を制御するスクリプトコンポーネント
class RailMovementScript : public ScriptableEntity {
public:
    float speed = 5.0f;
    std::vector<Waypoint> waypoints;
    int currentWaypointIndex = 0;
    bool isMoving = true;
    bool loop = false;

    bool drawPath = true; // デバッグ用パス描画フラグ
    RC::Vector4 pathColor = { 1.0f, 1.0f, 0.0f, 1.0f }; // パスの色（黄）

private:
    float currentWaitTimer = 0.0f;

protected:
    nlohmann::json Serialize() override {
        nlohmann::json j;
        j["speed"] = speed;
        j["isMoving"] = isMoving;
        j["loop"] = loop;
        j["drawPath"] = drawPath;
        j["pathColor"] = { pathColor.x, pathColor.y, pathColor.z, pathColor.w };
        nlohmann::json wpArray = nlohmann::json::array();
        for (const auto& wp : waypoints) {
            wpArray.push_back({
                {"pos", {wp.pos.x, wp.pos.y, wp.pos.z}},
                {"waitTime", wp.waitTime}
            });
        }
        j["waypoints"] = wpArray;
        return j;
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("speed")) speed = j["speed"].get<float>();
        if (j.contains("isMoving")) isMoving = j["isMoving"].get<bool>();
        if (j.contains("loop")) loop = j["loop"].get<bool>();
        if (j.contains("drawPath")) drawPath = j["drawPath"].get<bool>();
        if (j.contains("pathColor") && j["pathColor"].size() == 4) {
            pathColor.x = j["pathColor"][0];
            pathColor.y = j["pathColor"][1];
            pathColor.z = j["pathColor"][2];
            pathColor.w = j["pathColor"][3];
        }
        if (j.contains("waypoints") && j["waypoints"].is_array()) {
            waypoints.clear();
            for (const auto& wj : j["waypoints"]) {
                Waypoint wp;
                if (wj.contains("pos") && wj["pos"].size() == 3) {
                    wp.pos.x = wj["pos"][0];
                    wp.pos.y = wj["pos"][1];
                    wp.pos.z = wj["pos"][2];
                }
                if (wj.contains("waitTime")) wp.waitTime = wj["waitTime"].get<float>();
                waypoints.push_back(wp);
            }
        }
    }

    void OnCreate() override {
        // 初期化時に何もウェイポイントがなければ、現在位置を追加しておく
        if (waypoints.empty()) {
            if (auto* tr = GetComponent<TransformComponent>()) {
                waypoints.push_back({ tr->position, 0.0f });
            }
        }
    }

    void OnUpdate(float deltaTime) override {
        // パス描画処理はOnImGui(選択時)に移動しました。

        if (!isMoving || waypoints.empty() || currentWaypointIndex >= waypoints.size()) {
            return;
        }

        // ゲームオーバー判定
        if (Scene* scene = GetScene()) {
            for (auto& e : scene->GetEntities()) {
                if (e->GetTagInt("game_over", 0) == 1) {
                    return; // 移動停止
                }
            }
        }

        // 待機処理
        if (currentWaitTimer > 0.0f) {
            currentWaitTimer -= deltaTime;
            return; // 待機中は移動しない
        }

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        RC::Vector3 targetPos = waypoints[currentWaypointIndex].pos;
        RC::Vector3 diff = RC::Sub(targetPos, tr->position);
        float dist = RC::Length(diff);

        float moveAmount = speed * deltaTime;

        if (dist <= moveAmount) {
            // ターゲット地点に到達した
            tr->position = targetPos;
            
            // WaitTimeを設定
            currentWaitTimer = waypoints[currentWaypointIndex].waitTime;
            
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
        ImGui::Checkbox("Draw Path", &drawPath);
        if (drawPath) {
            ImGui::ColorEdit4("Path Color", &pathColor.x);
        }
        
        ImGui::Text("Waypoints (Count: %zu)", waypoints.size());
        ImGui::Text("Current Index: %d", currentWaypointIndex);
        if (currentWaitTimer > 0.0f) {
            ImGui::Text("Waiting... %.2f sec", currentWaitTimer);
        }

        if (ImGui::Button("Restart Path")) {
            currentWaypointIndex = 0;
            currentWaitTimer = 0.0f;
            if (!waypoints.empty()) {
                if (auto* tr = GetComponent<TransformComponent>()) {
                    tr->position = waypoints[0].pos;
                }
            }
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Add Waypoint")) {
            if (!waypoints.empty()) {
                // 最後の要素をコピーして追加
                waypoints.push_back(waypoints.back());
            } else if (auto* tr = GetComponent<TransformComponent>()) {
                // Transformの現在位置を追加
                waypoints.push_back({ tr->position, 0.0f });
            } else {
                waypoints.push_back({ {0,0,0}, 0.0f });
            }
        }

        ImGui::Separator();
        for (size_t i = 0; i < waypoints.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("WP %zu", i);
            ImGui::SameLine();
            ImGui::DragFloat3("##pos", &waypoints[i].pos.x, 0.1f);
            ImGui::SameLine();
            ImGui::DragFloat("Wait(s)", &waypoints[i].waitTime, 0.1f, 0.0f, 60.0f);
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

    void OnDebugRender() override {
#if RC_ENABLE_IMGUI
        // 選択時（または全描画モード時）のみパスとウェイポイントを描画
        if (drawPath && waypoints.size() >= 1) {
            for (size_t i = 0; i < waypoints.size(); ++i) {
                // 引数: center, radius, color, slices, stacks, depth
                RC::DrawWireSphere3D(waypoints[i].pos, 0.5f, pathColor, 16, 16, true);
                if (i < waypoints.size() - 1) {
                    RC::DrawLine3D(waypoints[i].pos, waypoints[i + 1].pos, pathColor, true);
                }
            }
            if (loop && waypoints.size() >= 2) {
                RC::DrawLine3D(waypoints.back().pos, waypoints.front().pos, pathColor, true);
            }
        }
#endif
    }
};

REGISTER_SCRIPT(RailMovementScript)
