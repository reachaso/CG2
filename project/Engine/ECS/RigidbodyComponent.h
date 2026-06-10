#pragma once
#include "IComponent.h"
#include "Entity.h"
#include "TransformComponent.h"
#include "Math/MathTypes.h"
#include <nlohmann/json.hpp>

class RigidbodyComponent : public IComponent {
public:
  bool useGravity = true;       ///< Whether gravity affects this rigidbody
  bool isKinematic = false;     ///< If true, physics doesn't update its position
  float mass = 1.0f;            ///< Mass of the object
  RC::Vector3 velocity = {0.0f, 0.0f, 0.0f}; ///< Current linear velocity

  const char* TypeName() const override { return "RigidbodyComponent"; }

  void Update(float deltaTime) override {
    if (!enabled_ || isKinematic) return;

    // Apply gravity
    if (useGravity) {
      // 簡易的な重力加速度 (m/s^2)
      constexpr float gravityAcc = -9.81f;
      velocity.y += gravityAcc * deltaTime;
    }

    // Update position
    if (entity_) {
      if (auto* tr = entity_->GetComponent<TransformComponent>()) {
        tr->position.x += velocity.x * deltaTime;
        tr->position.y += velocity.y * deltaTime;
        tr->position.z += velocity.z * deltaTime;
      }
    }
  }

  nlohmann::json Serialize() const override {
    return {
      {"useGravity", useGravity},
      {"isKinematic", isKinematic},
      {"mass", mass},
      {"velocity", {velocity.x, velocity.y, velocity.z}}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("useGravity")) useGravity = j["useGravity"].get<bool>();
    if (j.contains("isKinematic")) isKinematic = j["isKinematic"].get<bool>();
    if (j.contains("mass")) mass = j["mass"].get<float>();
    if (j.contains("velocity")) {
      auto& v = j["velocity"];
      velocity = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
    }
  }
};
