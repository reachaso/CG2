#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include "Math/Math.h"
#include <nlohmann/json.hpp>

/// @brief Component for position, rotation, and scale.
/// Supports parent-child hierarchy for world matrix calculation.
class TransformComponent : public IComponent {
public:
  RC::Vector3 position = {0.0f, 0.0f, 0.0f}; ///< Position (x, y, z)
  RC::Vector3 rotation = {0.0f, 0.0f, 0.0f}; ///< Rotation (Euler angles: x, y, z)
  RC::Vector3 scale = {1.0f, 1.0f, 1.0f};    ///< Scale (x, y, z)

  const char* TypeName() const override { return "TransformComponent"; }

  nlohmann::json Serialize() const override {
    return {
      {"position", {position.x, position.y, position.z}},
      {"rotation", {rotation.x, rotation.y, rotation.z}},
      {"scale",    {scale.x,    scale.y,    scale.z}}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("position")) {
      auto& p = j["position"];
      position = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
    }
    if (j.contains("rotation")) {
      auto& r = j["rotation"];
      rotation = {r[0].get<float>(), r[1].get<float>(), r[2].get<float>()};
    }
    if (j.contains("scale")) {
      auto& s = j["scale"];
      scale = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
    }
  }

  /// @brief Compute and return the world matrix
  /// @return Final world matrix considering parent transform
  RC::Matrix4x4 GetWorldMatrix() const {
    RC::Matrix4x4 local = MakeAffineMatrix(scale, rotation, position);
    if (parent_) {
      return Multiply(local, parent_->GetWorldMatrix());
    }
    return local;
  }

  /// @brief Set values from a Transform struct
  /// @param t Source Transform struct
  void SetFromTransform(const Transform &t) {
    scale = t.scale;
    rotation = t.rotation;
    position = t.translation;
  }

  /// @brief Convert to a Transform struct
  /// @return Transform struct with current values
  Transform ToTransform() const {
    Transform t;
    t.scale = scale;
    t.rotation = rotation;
    t.translation = position;
    return t;
  }

  /// @brief Set parent transform
  /// @param parent Pointer to parent component
  void SetParent(TransformComponent *parent) { parent_ = parent; }
  
  /// @brief Get parent transform
  /// @return Pointer to parent component, or nullptr
  TransformComponent *GetParent() const { return parent_; }

private:
  TransformComponent *parent_ = nullptr; ///< Non-owning pointer to parent
};
