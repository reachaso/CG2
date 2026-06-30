#pragma once
#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "RenderCommon.h"
#include "Scene.h"
#include <iostream>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief Base class for enemies handling HP, damage, and death state
class EnemyBaseScript : public ScriptableEntity {
public:
    int hp = 10;
    int maxHp = 10;
    bool isDead = false;
    float deathTimer = 0.0f;
    
    // Flash variables
    float flashTimer = 0.0f;
    RC::Vector4 originalColor = {1.0f, 1.0f, 1.0f, 1.0f};
    bool hasSavedColor = false;

protected:
    void SaveOriginalColor() {
        if (hasSavedColor) return;
        if (Entity* self = GetEntity()) {
            if (auto* mr = self->GetComponent<ModelRendererComponent>()) {
                originalColor = mr->color;
                hasSavedColor = true;
            } else if (auto* pm = self->GetComponent<PrimitiveMeshComponent>()) {
                if (pm->meshHandle >= 0) {
                    if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                        originalColor = mat->color;
                        hasSavedColor = true;
                    }
                }
            }
        }
    }

    void ApplyColor(const RC::Vector4& color) {
        if (Entity* self = GetEntity()) {
            if (auto* mr = self->GetComponent<ModelRendererComponent>()) {
                mr->color = color;
            } else if (auto* pm = self->GetComponent<PrimitiveMeshComponent>()) {
                if (pm->meshHandle >= 0) {
                    if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                        mat->color = color;
                    }
                }
            }
        }
    }

    void RestoreColor() {
        if (hasSavedColor) {
            ApplyColor(originalColor);
        }
    }

    void OnCreate() override {
        if (Entity* self = GetEntity()) {
            self->SetTag("is_enemy", 1);
        }
    }

    void OnUpdate(float deltaTime) override {
        // Process pending damage from tag system
        Entity* self = GetEntity();
        if (self) {
            int dmg = self->GetTagInt("pending_damage", 0);
            if (dmg > 0) {
                self->ClearTag("pending_damage");
                TakeDamage(dmg);
            }
            
            // Update flash timer
            if (flashTimer > 0.0f) {
                flashTimer -= deltaTime;
                if (flashTimer <= 0.0f) {
                    RestoreColor();
                }
            }

            // 仮の消滅処理
            if (isDead) {
                deathTimer += deltaTime;
                if (deathTimer > 2.0f) { // 2秒後に消滅
                    self->Destroy();
                }
            }
        }
    }

    void OnRender() override {
        DrawEnemyHPBar();
    }

public:
    void OnImGui() override {
#if RC_ENABLE_IMGUI
        ImGui::DragInt("HP##EnemyBase", &hp, 1, 0, maxHp);
#endif
    }

    virtual void TakeDamage(int damage) {
        if (isDead) return;
        hp -= damage;
        
        // ダメージを受けた時に赤色にフラッシュさせる
        SaveOriginalColor();
        ApplyColor({1.0f, 0.2f, 0.2f, 1.0f}); // 一時的に赤色に
        flashTimer = 0.1f; // 0.1秒後に元の色に戻る

        if (hp <= 0) {
            hp = 0;
            isDead = true;
            deathTimer = 0.0f;
            // タグでシーン側に撃破を通知
            if (Entity* self = GetEntity()) {
                self->SetTag("enemy_defeated", 1);
            }
        }
    }

    virtual void DrawEnemyHPBar() {
        if (isDead) return;
        SceneContext* ctx = GetSceneContext();
        if (!ctx) return;

        float screenW = static_cast<float>(ctx->app->width);
        // Draw HP bar at top-right
        float barX = screenW - 230.0f;
        float barY = 60.0f;
        float barW = 200.0f;
        float barH = 16.0f;

        // Background
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH },
                    { 0.3f, 0.05f, 0.05f, 0.8f });

        // Foreground
        float hpRatio = static_cast<float>(hp) / static_cast<float>(maxHp);
        RC::Vector4 hpColor = { 0.9f, 0.2f, 0.2f, 0.9f };
        RC::DrawBox({ barX, barY }, { barX + barW * hpRatio, barY + barH }, hpColor);

        // Border
        RC::DrawBox({ barX, barY }, { barX + barW, barY + barH },
                    { 1.0f, 1.0f, 1.0f, 0.5f }, kWire);
    }
};
