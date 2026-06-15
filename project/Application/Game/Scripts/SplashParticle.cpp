#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "RenderCommon.h"
#include "Scene.h"
#include <cmath>

/// @brief Visual splash particle: flies outward and fades
class SplashParticle : public ScriptableEntity {
protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
        lifetime_ = 0.6f + static_cast<float>(rand() % 100) / 100.0f;

        // Random velocity (outward + upward)
        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float speed = 4.0f + static_cast<float>(rand() % 100) / 15.0f;
        float upSpeed = 4.0f + static_cast<float>(rand() % 100) / 15.0f;

        velX_ = std::cos(angle) * speed;
        velY_ = upSpeed;
        velZ_ = std::sin(angle) * speed;
    }

    void OnUpdate(float deltaTime) override {
        if (markedForDestroy_) return;

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        if (!initScale_) {
            startScale_ = tr->scale.x;
            initScale_ = true;
        }

        elapsed_ += deltaTime;
        if (elapsed_ >= lifetime_) {
            Cleanup();
            return;
        }

        // Gravity
        velY_ -= 12.0f * deltaTime;

        // Move
        tr->position.x += velX_ * deltaTime;
        tr->position.y += velY_ * deltaTime;
        tr->position.z += velZ_ * deltaTime;

        // Below water: destroy
        if (tr->position.y < -0.5f) {
            Cleanup();
            return;
        }

        // Shrink over time
        float t = elapsed_ / lifetime_;
        float scale = (1.0f - t) * startScale_;
        if (scale < 0.01f) scale = 0.01f;
        tr->scale = { scale, scale, scale };

        // Fade alpha
        auto* pm = GetComponent<PrimitiveMeshComponent>();
        if (pm && pm->meshHandle >= 0) {
            if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                mat->color.w = (1.0f - t) * 0.85f;
            }
        }
    }

private:
    float elapsed_ = 0.0f;
    float lifetime_ = 0.5f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    float velZ_ = 0.0f;
    bool markedForDestroy_ = false;
    bool initScale_ = false;
    float startScale_ = 1.0f;

    void Cleanup() {
        if (markedForDestroy_) return;
        markedForDestroy_ = true;

        Entity* self = GetEntity();
        Scene* scene = GetScene();
        if (self && scene) {
            auto* pm = self->GetComponent<PrimitiveMeshComponent>();
            if (pm && pm->meshHandle >= 0) {
                RC::UnloadPrimitiveMesh(pm->meshHandle);
                pm->meshHandle = -1;
            }
            scene->RemoveEntity(self->GetId());
        }
    }
};

REGISTER_SCRIPT(SplashParticle)

/// @brief Wake particle: behaves like a small bouncing water droplet
class WakeParticle : public ScriptableEntity {
protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
        lifetime_ = 0.5f + static_cast<float>(rand() % 50) / 100.0f; // 0.5 ~ 1.0s

        // ランダムな初速（上方向＋わずかに横）
        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float speed = 0.5f + static_cast<float>(rand() % 100) / 100.0f;
        velX_ = std::cos(angle) * speed;
        velZ_ = std::sin(angle) * speed;
        velY_ = 3.0f + static_cast<float>(rand() % 100) / 50.0f; // 3.0 ~ 5.0
    }

    void OnUpdate(float deltaTime) override {
        if (markedForDestroy_) return;

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        if (!initScale_) {
            startScale_ = tr->scale.x;
            initScale_ = true;
        }

        elapsed_ += deltaTime;
        if (elapsed_ >= lifetime_) {
            Cleanup();
            return;
        }

        // 重力と移動
        velY_ -= 15.0f * deltaTime;
        tr->position.x += velX_ * deltaTime;
        tr->position.y += velY_ * deltaTime;
        tr->position.z += velZ_ * deltaTime;

        // 水面に落ちたらバウンドするか消える
        if (tr->position.y < 0.0f) {
            tr->position.y = 0.0f;
            velY_ *= -0.3f; // 少しバウンド
            velX_ *= 0.8f;
            velZ_ *= 0.8f;
        }

        // 徐々に小さくなる
        float t = elapsed_ / lifetime_;
        float currentScale = startScale_ * (1.0f - t);
        if (currentScale < 0.01f) currentScale = 0.01f;
        tr->scale = { currentScale, currentScale, currentScale };

        auto* pm = GetComponent<PrimitiveMeshComponent>();
        if (pm && pm->meshHandle >= 0) {
            if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                mat->color.w = (1.0f - t) * 0.9f; 
            }
            if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                pmTr->scale = tr->scale;
                pmTr->translation = tr->position;
            }
        }
    }

private:
    float elapsed_ = 0.0f;
    float lifetime_ = 1.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    float velZ_ = 0.0f;
    bool markedForDestroy_ = false;
    bool initScale_ = false;
    float startScale_ = 1.0f;

    void Cleanup() {
        if (markedForDestroy_) return;
        markedForDestroy_ = true;

        Entity* self = GetEntity();
        Scene* scene = GetScene();
        if (self && scene) {
            auto* pm = self->GetComponent<PrimitiveMeshComponent>();
            if (pm && pm->meshHandle >= 0) {
                RC::UnloadPrimitiveMesh(pm->meshHandle);
                pm->meshHandle = -1;
            }
            scene->RemoveEntity(self->GetId());
        }
    }
};

REGISTER_SCRIPT(WakeParticle)
