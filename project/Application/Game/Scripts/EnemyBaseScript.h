#pragma once
#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "Scene.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/GPUParticleComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "Particle/GPUParticle.h"
#include "RenderCommon.h"
#include <iostream>
#include <format>
#include "Common/Log/Log.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include "Engine/Camera/CameraMath.h"
#include "ECS/CameraComponent.h"
#include "ECS/ColliderComponent.h"
#include "Engine/Render/RenderContext.h"


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
            
            // コライダーがなければフェールセーフとして追加
            if (!self->HasComponent<ColliderComponent>()) {
                auto* col = &self->AddComponent<ColliderComponent>();
                col->shape = ColliderComponent::Shape::Sphere;
                col->radius = 1.0f; // スケール依存
                col->isTrigger = false;
            }
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
            
            // HP情報をタグで公開（UI用）
            self->SetTag("current_hp", hp);
            self->SetTag("max_hp", maxHp);
        }
    }

    void OnRender() override {
        // UI描画は RailShooterController 側で一括して行うように変更しました
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
        
        Log::Print(std::format("[{}] Took {} damage! HP left: {}/{}", GetEntity()->GetName(), damage, hp, maxHp));
        
        // ダメージを受けた時に赤色にフラッシュさせる
        SaveOriginalColor();
        ApplyColor({1.0f, 0.2f, 0.2f, 1.0f}); // 一時的に赤色に
        flashTimer = 0.2f; // 0.2秒後に元の色に戻る

        if (hp <= 0) {
            hp = 0;
            isDead = true;
            deathTimer = 0.0f;
            // タグでシーン側に撃破を通知
            if (Entity* self = GetEntity()) {
                self->SetTag("enemy_defeated", 1);
                
                // 撃破後は弾が当たらないようにする
                self->ClearTag("is_enemy");
                if (auto* col = self->GetComponent<ColliderComponent>()) {
                    col->SetEnabled(false);
                }

                // GPU泡パーティクル発生
                if (Scene* scene = GetScene()) {
                    auto emitter = scene->CreateEntity("GPUBubbleEmitter");
                    auto* tr = &emitter->AddComponent<TransformComponent>();
                    if (auto* myTr = self->GetComponent<TransformComponent>()) {
                        tr->position = myTr->position;
                    }
                    auto* gpu = &emitter->AddComponent<GPUParticleComponent>();
                    if (gpu->particleSystem) {
                        gpu->particleSystem->SetPipelinePrefix("gpu_particle_bubble");
                    }
                    
                    auto* nsc = &emitter->AddComponent<NativeScriptComponent>();
                    nsc->AddScript("GPUBubbleEmitter");
                    nsc->SetScene(scene);
                    if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());

                    scene->InitDynamicEntityRuntime(*emitter);
                }
            }
        }
    }

};
