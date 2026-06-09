#pragma once

#include "IComponent.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief Animation control component.
/// Used in combination with ModelRendererComponent.
/// Manages animation playback/stop, speed control, and skeleton debug display.
class AnimationComponent : public IComponent {
public:
  bool playing = true;         ///< Playing flag
  float speed = 1.0f;          ///< Playback speed (1.0 = normal)
  bool showSkeleton = false;   ///< Skeleton debug display flag
  std::string animationPath;   ///< External animation file path (empty = use model's built-in)
  bool attached_ = false;      ///< Whether animation is attached (internal)

  const char* TypeName() const override { return "AnimationComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"playing", playing},
      {"speed", speed},
      {"showSkeleton", showSkeleton},
      {"animationPath", animationPath}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("playing")) playing = j["playing"].get<bool>();
    if (j.contains("speed")) speed = j["speed"].get<float>();
    if (j.contains("showSkeleton")) showSkeleton = j["showSkeleton"].get<bool>();
    if (j.contains("animationPath")) animationPath = j["animationPath"].get<std::string>();
  }
};
