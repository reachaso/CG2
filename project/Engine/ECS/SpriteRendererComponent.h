#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include <string>
#include <nlohmann/json.hpp>

/// @brief Component for 2D sprite rendering.
/// Holds a sprite handle and screen size, automatically drawn in the entity loop.
class SpriteRendererComponent : public IComponent {
public:
  int spriteHandle = -1;  ///< Sprite handle (from RC::LoadSprite)
  bool visible = true;    ///< Visibility flag
  RC::Vector2 size = {100.0f, 100.0f}; ///< Display size (screen coords, pixels)
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< Multiply color

  /// @brief Check if a valid sprite is assigned
  bool HasSprite() const { return spriteHandle >= 0; }

  std::string spritePath; ///< Asset path for serialization

  const char* TypeName() const override { return "SpriteRendererComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"spritePath", spritePath},
      {"visible", visible},
      {"size", {size.x, size.y}},
      {"color", {color.x, color.y, color.z, color.w}}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("spritePath")) spritePath = j["spritePath"].get<std::string>();
    if (j.contains("visible")) visible = j["visible"].get<bool>();
    if (j.contains("size")) {
      auto& s = j["size"];
      size = {s[0].get<float>(), s[1].get<float>()};
    }
    if (j.contains("color")) {
      auto& c = j["color"];
      color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
    }
  }
};
