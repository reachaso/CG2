#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief Skybox rendering component.
/// Holds a handle created by RC::CreateSkyBox().
class SkyboxComponent : public IComponent {
public:
  int skyboxHandle = -1;  ///< Skybox handle
  bool visible = true;    ///< Visibility flag
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< Multiply color

  /// @brief Check if a valid handle is assigned
  bool HasSkybox() const { return skyboxHandle >= 0; }

  std::string skyboxPath; ///< Asset path for serialization

  const char* TypeName() const override { return "SkyboxComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"skyboxPath", skyboxPath},
      {"visible", visible},
      {"color", {color.x, color.y, color.z, color.w}}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("skyboxPath")) skyboxPath = j["skyboxPath"].get<std::string>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("color")) {
      auto& c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
  }
};
