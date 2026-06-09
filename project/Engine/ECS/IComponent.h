#pragma once
#include <nlohmann/json_fwd.hpp>

/// @brief Base interface for all components.
/// All components attached to an Entity must inherit from this class.
class IComponent {
public:
  virtual ~IComponent() = default;

  /// @brief Per-frame update
  /// @param deltaTime Time elapsed since last frame (seconds)
  virtual void Update(float deltaTime) { (void)deltaTime; }

  /// @brief Get component type name (for serialization)
  /// @return Type name string (e.g. "TransformComponent")
  virtual const char* TypeName() const = 0;

  /// @brief Serialize component state to JSON
  /// @return Serialized JSON object
  virtual nlohmann::json Serialize() const = 0;

  /// @brief Deserialize component state from JSON
  /// @param j Source JSON object
  virtual void Deserialize(const nlohmann::json& j) = 0;

  /// @brief Get enabled state
  /// @return true if enabled
  bool IsEnabled() const { return enabled_; }
  
  /// @brief Set enabled state
  /// @param enabled State to set
  void SetEnabled(bool enabled) { enabled_ = enabled; }

protected:
  bool enabled_ = true; ///< Enabled flag (Update skipped if false)
};
