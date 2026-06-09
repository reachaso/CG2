#pragma once

#include "IComponent.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief Primitive mesh shape type
enum class PrimitiveType {
  Sphere,
  Box,
  Plane,
  Cylinder,
  Cone,
  Torus,
  Capsule,
};

/// @brief Component for primitive mesh rendering.
/// Holds shape type and size parameters, automatically drawn in the entity loop.
class PrimitiveMeshComponent : public IComponent {
public:
  int meshHandle = -1;    ///< Mesh handle (from RC::Generate*)
  int texOverride = -1;   ///< Texture override (-1 for default)
  bool visible = true;    ///< Visibility flag
  PrimitiveType type = PrimitiveType::Sphere; ///< Shape type (for Inspector)
  float environmentCoeff = 0.0f; ///< Environment map reflection coefficient

  /// @brief Check if a valid mesh is assigned
  bool HasMesh() const { return meshHandle >= 0; }

  const char* TypeName() const override { return "PrimitiveMeshComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"type", static_cast<int>(type)},
      {"texOverride", texOverride},
      {"visible", visible},
      {"environmentCoeff", environmentCoeff}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("type")) type = static_cast<PrimitiveType>(j["type"].get<int>());
    if (j.contains("texOverride")) texOverride = j["texOverride"].get<int>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("environmentCoeff")) environmentCoeff = j["environmentCoeff"].get<float>();
  }
};
