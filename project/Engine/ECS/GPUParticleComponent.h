#pragma once

#include "IComponent.h"
#include "Particle/GPUParticle.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

/// @brief Component for rendering GPU particles on an Entity.
class GPUParticleComponent : public IComponent {
public:
  std::unique_ptr<GPUParticle> particleSystem;
  bool isInitialized = false;

  std::string configPath = "Resources/Particle/default.json";
  std::string texturePath = "Resources/Particle/circle.png";

  GPUParticleComponent() {
    particleSystem = std::make_unique<GPUParticle>();
  }

  const char* TypeName() const override { return "GPUParticleComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"configPath", configPath},
      {"texturePath", texturePath}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("configPath")) {
      configPath = j["configPath"].get<std::string>();
    }
    if (j.contains("texturePath")) {
      texturePath = j["texturePath"].get<std::string>();
    }
  }
};
