#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <nlohmann/json.hpp>

/// @brief Component for 3D model rendering.
/// Holds a model handle and is automatically drawn by the RenderSystem.
class ModelRendererComponent : public IComponent {
public:
  int modelHandle = -1;   ///< Model handle (returned by RC::LoadModel())
  int texOverride = -1;   ///< Texture override (-1 for default)
  int normalMapOverride = -1; ///< Normal Map texture override
  int roughnessMapOverride = -1; ///< Roughness Map texture override
  bool visible = true;    ///< Visibility flag
  int blendMode = 0;      ///< Blend mode
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< Multiply color
  float environmentCoeff = 0.0f; ///< Environment map reflection coefficient

  /// @brief Check if a valid model is assigned
  bool HasModel() const { return modelHandle >= 0; }

  std::string modelPath; ///< Asset path for serialization
  std::string texturePath; ///< Texture asset path for serialization
  std::string normalMapPath; ///< Normal Map asset path for serialization
  std::string roughnessMapPath; ///< Roughness Map asset path for serialization

  const char* TypeName() const override { return "ModelRendererComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"modelPath", modelPath},
      {"texturePath", texturePath},
      {"normalMapPath", normalMapPath},
      {"roughnessMapPath", roughnessMapPath},
      {"visible", visible},
      {"blendMode", blendMode},
      {"color", {color.x, color.y, color.z, color.w}},
      {"environmentCoeff", environmentCoeff}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("modelPath")) modelPath = j["modelPath"].get<std::string>();
    if (j.contains("texturePath")) texturePath = j["texturePath"].get<std::string>();
    if (j.contains("normalMapPath")) normalMapPath = j["normalMapPath"].get<std::string>();
    if (j.contains("roughnessMapPath")) roughnessMapPath = j["roughnessMapPath"].get<std::string>();
    // Backward compatibility for texOverride if needed
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("blendMode")) blendMode = j["blendMode"].get<int>();
    if (j.contains("color")) {
      auto& c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
    if (j.contains("environmentCoeff")) environmentCoeff = j["environmentCoeff"].get<float>();
  }
};
