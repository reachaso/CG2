#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief Skydome rendering component.
/// Holds a handle created by RC::GenerateSkydomeEx() etc.
class SkydomeComponent : public IComponent {
public:
  int skydomeHandle = -1; ///< Skydome handle
  bool visible = true;    ///< Visibility flag
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< Multiply color
  int texOverride = -1;   ///< Texture override (-1 for default)

  /// @brief Check if a valid handle is assigned
  bool HasSkydome() const { return skydomeHandle >= 0; }

  std::string skydomePath; ///< Asset path for serialization
  std::string texturePath; ///< Texture asset path for serialization

  const char* TypeName() const override { return "SkydomeComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"skydomePath", skydomePath},
      {"visible", visible},
      {"color", {color.x, color.y, color.z, color.w}},
      {"texturePath", texturePath}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("skydomePath")) skydomePath = j["skydomePath"].get<std::string>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("color")) {
      auto& c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
    if (j.contains("texturePath")) texturePath = j["texturePath"].get<std::string>();
  }
};
