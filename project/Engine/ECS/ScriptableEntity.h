#pragma once
#include "Entity.h"

/// @brief Base class for native scripts attached to entities.
/// Allows writing custom update logic similar to Unity's MonoBehaviour.
class ScriptableEntity {
public:
  virtual ~ScriptableEntity() = default;

  /// @brief Called for drawing custom ImGui UI in the editor
  virtual void OnImGui() {}

  /// @brief Get a component attached to the same entity
  template <typename T> T* GetComponent() {
    return entity_->GetComponent<T>();
  }

  /// @brief Check if a component is attached to the same entity
  template <typename T> bool HasComponent() {
    return entity_->HasComponent<T>();
  }

  /// @brief Add a component to the same entity
  template <typename T, typename... Args> T& AddComponent(Args&&... args) {
    return entity_->AddComponent<T>(std::forward<Args>(args)...);
  }

  /// @brief Get the entity this script is attached to
  Entity* GetEntity() const { return entity_; }

protected:
  /// @brief Called when the script is created
  virtual void OnCreate() {}
  
  /// @brief Called every frame
  /// @param deltaTime Time elapsed since the last frame
  virtual void OnUpdate(float deltaTime) {}
  
  /// @brief Called when the script is destroyed
  virtual void OnDestroy() {}

private:
  Entity* entity_ = nullptr;
  friend class NativeScriptComponent;
};
