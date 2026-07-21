#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "RenderCommon.h"
#include "Scene.h"
#include "ECS/GPUParticleComponent.h"
#include "Particle/GPUParticle.h"
#include <cmath>

/// @brief Visual splash particle: flies outward and fades
class SplashParticle : public ScriptableEntity {
protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
        lifetime_ = 0.6f + static_cast<float>(rand() % 100) / 100.0f;

        float impactFactor = 1.0f;
        if (Entity* self = GetEntity()) {
            int scaleTag = self->GetTagInt("impact_factor", 100);
            impactFactor = static_cast<float>(scaleTag) / 100.0f;
        }

        // Random velocity (outward + upward)
        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float speed = (4.0f + static_cast<float>(rand() % 100) / 15.0f) * std::sqrt(impactFactor);
        float upSpeed = (4.0f + static_cast<float>(rand() % 100) / 15.0f) * impactFactor;

        velX_ = std::cos(angle) * speed;
        velY_ = upSpeed;
        velZ_ = std::sin(angle) * speed;
    }

    void OnUpdate(float deltaTime) override {
        Entity* self = GetEntity();
        if (self && self->GetTagInt("reused", 0) == 1) {
            OnCreate();
            initScale_ = false;
            markedForDestroy_ = false;
            self->ClearTag("reused");
        }

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
        if (self) {
            self->SetActive(false);
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
        Entity* self = GetEntity();
        if (self && self->GetTagInt("reused", 0) == 1) {
            OnCreate();
            initScale_ = false;
            markedForDestroy_ = false;
            self->ClearTag("reused");
        }

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
        if (self) {
            self->SetActive(false);
        }
    }
};

REGISTER_SCRIPT(WakeParticle)

/// @brief Heavy splash particle: high velocity upward water column
class HeavySplashParticle : public ScriptableEntity {
protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
        lifetime_ = 1.0f + static_cast<float>(rand() % 20) / 100.0f; // 1.0 ~ 1.2s

        float impactFactor = 1.0f;
        if (Entity* self = GetEntity()) {
            int scaleTag = self->GetTagInt("impact_factor", 100);
            impactFactor = static_cast<float>(scaleTag) / 100.0f;
        }

        // The column will grow up to this height
        maxHeight_ = 15.0f * impactFactor;
    }

    void OnUpdate(float deltaTime) override {
        Entity* self = GetEntity();
        if (self && self->GetTagInt("reused", 0) == 1) {
            OnCreate();
            initScale_ = false;
            markedForDestroy_ = false;
            self->ClearTag("reused");
        }

        if (markedForDestroy_) return;

        auto* tr = GetComponent<TransformComponent>();
        if (!tr) return;

        if (!initScale_) {
            startScale_ = tr->scale.x; // base width
            startY_ = tr->position.y;
            initScale_ = true;
        }

        elapsed_ += deltaTime;
        if (elapsed_ >= lifetime_) {
            Cleanup();
            return;
        }

        float t = elapsed_ / lifetime_;
        
        // Rapidly grow height, then hold, then shrink slightly
        // We use a custom ease out for height
        float heightProgress = 1.0f - std::pow(1.0f - t, 4.0f);
        float currentHeight = maxHeight_ * heightProgress;
        
        // Base width expands slightly then shrinks
        float widthScale = startScale_ * (1.0f + std::sin(t * 3.14159f) * 0.5f);

        tr->scale = { widthScale, currentHeight, widthScale };
        
        // Adjust position so the base stays at ground level
        // Cylinder mesh is centered at 0, so moving up by half height keeps bottom fixed
        tr->position.y = startY_ + currentHeight * 0.5f;

        auto* pm = GetComponent<PrimitiveMeshComponent>();
        if (pm && pm->meshHandle >= 0) {
            if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                // Fade out near the end
                float alpha = 1.0f;
                if (t > 0.7f) {
                    alpha = 1.0f - (t - 0.7f) / 0.3f;
                }
                mat->color.w = alpha;
                
                // Scroll UV for the rushing water effect (V axis)
                mat->uvTransform.m[3][1] -= deltaTime * 3.0f;
            }
            if (auto* pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
                pmTr->scale = tr->scale;
                pmTr->translation = tr->position;
                pmTr->rotation = tr->rotation;
            }
        }
    }

private:
    float elapsed_ = 0.0f;
    float lifetime_ = 1.0f;
    float maxHeight_ = 10.0f;
    float startY_ = 0.0f;
    bool markedForDestroy_ = false;
    bool initScale_ = false;
    float startScale_ = 1.0f;

    void Cleanup() {
        if (markedForDestroy_) return;
        markedForDestroy_ = true;
        Entity* self = GetEntity();
        if (self) {
            self->SetActive(false);
        }
    }
};

REGISTER_SCRIPT(HeavySplashParticle)

/// @brief Bubble particle: floating upwards slowly and popping
class BubbleParticle : public ScriptableEntity {
protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
        lifetime_ = 0.4f + static_cast<float>(rand() % 60) / 100.0f; // 0.4 ~ 1.0s

        float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
        float speed = 0.5f + static_cast<float>(rand() % 100) / 100.0f;
        velX_ = std::cos(angle) * speed;
        velZ_ = std::sin(angle) * speed;
        velY_ = 1.0f + static_cast<float>(rand() % 100) / 50.0f; // Slow upward
        
        baseVelY_ = velY_;
    }

    void OnUpdate(float deltaTime) override {
        Entity* self = GetEntity();
        if (self && self->GetTagInt("reused", 0) == 1) {
            OnCreate();
            initScale_ = false;
            markedForDestroy_ = false;
            self->ClearTag("reused");
        }

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

        // Wobble horizontally
        float wobble = std::sin(elapsed_ * 10.0f) * 0.5f;

        tr->position.x += (velX_ + wobble * velZ_) * deltaTime;
        tr->position.y += velY_ * deltaTime;
        tr->position.z += (velZ_ - wobble * velX_) * deltaTime;

        // Shrink at the very end to simulate popping
        float t = elapsed_ / lifetime_;
        float scale = startScale_;
        if (t > 0.8f) {
            scale = startScale_ * (1.0f - (t - 0.8f) * 5.0f);
        }
        if (scale < 0.01f) scale = 0.01f;
        tr->scale = { scale, scale, scale };

        auto* pm = GetComponent<PrimitiveMeshComponent>();
        if (pm && pm->meshHandle >= 0) {
            if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
                // Bubbles are more transparent and whitish
                mat->color = { 0.8f, 0.9f, 1.0f, (1.0f - t) * 0.6f };
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
    float baseVelY_ = 0.0f;
    bool markedForDestroy_ = false;
    bool initScale_ = false;
    float startScale_ = 1.0f;

    void Cleanup() {
        if (markedForDestroy_) return;
        markedForDestroy_ = true;
        Entity* self = GetEntity();
        if (self) {
            self->SetActive(false);
        }
    }
};

REGISTER_SCRIPT(BubbleParticle)

/// @brief GPUBubbleEmitter: Emits GPU bubble particles and destroys itself
class GPUBubbleEmitter : public ScriptableEntity {
protected:
    void OnCreate() override {
        elapsed_ = 0.0f;
        lifetime_ = 2.0f; // 2秒間エミッターとして存在

        if (Entity* self = GetEntity()) {
            if (auto* gpu = self->GetComponent<GPUParticleComponent>()) {
                if (gpu->particleSystem) {
                    gpu->particleSystem->SetMaxParticles(1024);
                    gpu->particleSystem->SetParticleType(ParticleType::Explosion);
                    gpu->particleSystem->SetBlendMode(kBlendModeNormal);
                    gpu->particleSystem->minScale_ = 0.5f;
                    gpu->particleSystem->maxScale_ = 1.5f;
                    // 色付きの泡など、基本は白
                    gpu->particleSystem->startColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; 
                    gpu->particleSystem->endColor_ = {1.0f, 1.0f, 1.0f, 0.0f};
                    gpu->particleSystem->SetEmitCount(100); // 最初の一瞬で一気に出す
                    gpu->particleSystem->minLifeTime_ = 1.0f;
                    gpu->particleSystem->maxLifeTime_ = 2.5f;
                    gpu->particleSystem->gravity_ = -3.0f; // 上に向かって浮上
                    gpu->particleSystem->baseVelocity_ = {0.0f, 2.0f, 0.0f};
                    gpu->particleSystem->velocityVariance_ = 0.5f;
                    gpu->particleSystem->emitterShape_ = EmitterShape::Sphere;
                    gpu->particleSystem->shapeRadius_ = 1.0f;
                }
            }
        }
    }

    void OnUpdate(float deltaTime) override {
        if (markedForDestroy_) return;

        elapsed_ += deltaTime;

        // 生成後すぐに emitCount を 0 にして放出を止める（バースト放出のシミュレート）
        if (elapsed_ > 0.1f) {
            if (Entity* self = GetEntity()) {
                if (auto* gpu = self->GetComponent<GPUParticleComponent>()) {
                    if (gpu->particleSystem) {
                        gpu->particleSystem->SetEmitCount(0);
                    }
                }
            }
        }

        if (elapsed_ >= lifetime_) {
            markedForDestroy_ = true;
            if (Entity* self = GetEntity()) {
                self->Destroy(); // 自身を破棄
            }
        }
    }

private:
    float elapsed_ = 0.0f;
    float lifetime_ = 2.0f;
    bool markedForDestroy_ = false;
};

REGISTER_SCRIPT(GPUBubbleEmitter)
