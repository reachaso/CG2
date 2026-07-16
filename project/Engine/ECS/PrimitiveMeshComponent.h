#pragma once

#include "IComponent.h"
#include <string>
#include <nlohmann/json.hpp>
#include "Math/MathTypes.h"

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
  int normalMapOverride = -1; ///< Normal Map texture override
  int roughnessMapOverride = -1; ///< Roughness Map texture override
  bool visible = true;    ///< Visibility flag
  PrimitiveType type = PrimitiveType::Sphere; ///< Shape type (for Inspector)
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< Multiply color
  float environmentCoeff = 0.0f; ///< Environment map reflection coefficient

  /// @brief Check if a valid mesh is assigned
  bool HasMesh() const { return meshHandle >= 0; }

  std::string texturePath; ///< Texture asset path for serialization
  std::string normalMapPath; ///< Normal Map asset path for serialization
  std::string roughnessMapPath; ///< Roughness Map asset path for serialization

  const char* TypeName() const override { return "PrimitiveMeshComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"type", static_cast<int>(type)},
      {"texturePath", texturePath},
      {"normalMapPath", normalMapPath},
      {"roughnessMapPath", roughnessMapPath},
      {"visible", visible},
      {"color", {color.x, color.y, color.z, color.w}},
      {"environmentCoeff", environmentCoeff}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("type")) type = static_cast<PrimitiveType>(j["type"].get<int>());
    if (j.contains("texturePath")) texturePath = j["texturePath"].get<std::string>();
    if (j.contains("normalMapPath")) normalMapPath = j["normalMapPath"].get<std::string>();
    if (j.contains("roughnessMapPath")) roughnessMapPath = j["roughnessMapPath"].get<std::string>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("color")) {
      auto& c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
    if (j.contains("environmentCoeff")) environmentCoeff = j["environmentCoeff"].get<float>();
  }
};
