#pragma once
#include "IComponent.h"
#include <nlohmann/json.hpp>

/// @brief Camera projection parameter component.
/// Attach to an Entity along with TransformComponent to make it function as a camera.
struct CameraComponent : public IComponent {
    float fovY = 0.45f;       ///< Vertical field of view (radians)
    float nearZ = 0.1f;       ///< Near clip distance
    float farZ = 100.0f;      ///< Far clip distance
    bool isMain = true;       ///< Main camera flag (for future multi-camera support)
    RC::Vector3 shakeOffset = {0.0f, 0.0f, 0.0f}; ///< Camera shake offset applied to position

    const char* TypeName() const override { return "CameraComponent"; }

    nlohmann::json Serialize() const override {
      return {
        {"fovY", fovY}, {"nearZ", nearZ}, {"farZ", farZ}, {"isMain", isMain}
      };
    }

    void Deserialize(const nlohmann::json& j) override {
      if (j.contains("fovY"))   fovY   = j["fovY"].get<float>();
      if (j.contains("nearZ"))  nearZ  = j["nearZ"].get<float>();
      if (j.contains("farZ"))   farZ   = j["farZ"].get<float>();
      if (j.contains("isMain")) isMain = j["isMain"].get<bool>();
    }
};
