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
        lifetime_ = 0.5f + static_cast<float>(rand() % 100) / 200.0f;

        // Random velocity (outward + upward)
        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float speed = 3.0f + static_cast<float>(rand() % 100) / 20.0f;
        float upSpeed = 3.0f + static_cast<float>(rand() % 100) / 25.0f;

        velX_ = std::cos(angle) * speed;
        velY_ = upSpeed;
        velZ_ = std::sin(angle) * speed;
    }

    void OnUpdate(float deltaTime) override {
        if (markedForDestroy_) return;

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        elapsed_ += deltaTime;
        if (elapsed_ >= lifetime_) {
            Cleanup();
            return;
        }

        // Gravity
        velY_ -= 15.0f * deltaTime;

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
        float scale = (1.0f - t) * tr->scale.x;
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
