#pragma once
#include "IComponent.h"
#include "Math/MathTypes.h"
#include "struct.h" // AABB
#include <nlohmann/json.hpp>

/// @brief Collision detection component.
/// Manages shape type (AABB, Sphere, Capsule), parameters, and collision layers.
class ColliderComponent : public IComponent {
public:
  /// @brief Collider shape definition
  enum class Shape {
    AABB,    ///< Axis-aligned bounding box
    Sphere,  ///< Sphere
    Capsule, ///< Capsule
  };

  Shape shape = Shape::AABB; ///< Current collider shape

  AABB aabb; ///< AABB parameters

  RC::Vector3 center = {0.0f, 0.0f, 0.0f}; ///< Center position (local)
  float radius = 1.0f;                      ///< Radius

  uint32_t layer = 0xFFFFFFFF; ///< Collision layer (bitmask)

  /// @brief Trigger mode. If true, no physics response, only event notification.
  bool isTrigger = false;

  const char* TypeName() const override { return "ColliderComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"shape", static_cast<int>(shape)},
      {"center", {center.x, center.y, center.z}},
      {"radius", radius},
      {"layer", layer},
      {"isTrigger", isTrigger}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("shape")) shape = static_cast<Shape>(j["shape"].get<int>());
    if (j.contains("center")) {
      auto& c = j["center"];
      center = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
    }
    if (j.contains("radius")) radius = j["radius"].get<float>();
    if (j.contains("layer")) layer = j["layer"].get<uint32_t>();
    if (j.contains("isTrigger")) isTrigger = j["isTrigger"].get<bool>();
  }
};
