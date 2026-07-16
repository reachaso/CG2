#pragma once
#include "Entity.h"
#include "Common/Math/MathUtils.h"

// Forward declarations
class Scene;
struct SceneContext;

/// @brief Base class for native scripts attached to entities.
/// Allows writing custom update logic similar to Unity's MonoBehaviour.
class ScriptableEntity {
public:
  virtual ~ScriptableEntity() = default;

  /// @brief Serialize script data to JSON
  virtual nlohmann::json Serialize() { return nlohmann::json::object(); }

  /// @brief Deserialize script data from JSON
  virtual void Deserialize(const nlohmann::json& j) {}

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

  /// @brief Get the scene this script belongs to
  Scene* GetScene() const { return scene_; }

  /// @brief Get the current scene context
  SceneContext* GetSceneContext() const { return sceneContext_; }

  /// @brief Called during the render phase (command list is open)
  /// @details Use this for 2D drawing (DrawBox, DrawCircle, etc.)
  virtual void OnRender() {}

  /// @brief Called during the overlay 3D phase to draw gizmos
  virtual void OnDebugRender() {}

  /// @brief Called when a collision occurs with another entity
  /// @param other The entity that this entity collided with
  /// @param contactPoint The world position where the collision occurred
  virtual void OnCollision(Entity* other, const RC::Vector3& contactPoint = {}) {}

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
  Scene* scene_ = nullptr;
  SceneContext* sceneContext_ = nullptr;
  friend class NativeScriptComponent;
};
