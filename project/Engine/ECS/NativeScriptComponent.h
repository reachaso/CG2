#pragma once
#include "IComponent.h"
#include "ScriptableEntity.h"
#include "ScriptRegistry.h"
#include <functional>
#include <string>

/// @brief Component that manages and executes a native script (ScriptableEntity).
class NativeScriptComponent : public IComponent {
public:
  ScriptableEntity* instance = nullptr;

  std::function<ScriptableEntity*()> InstantiateScript;
  std::function<void(NativeScriptComponent*)> DestroyScript;

  std::string scriptTypeName;

  ~NativeScriptComponent() override {
    if (instance && DestroyScript) {
      instance->OnDestroy();
      DestroyScript(this);
    }
  }

  /// @brief Bind a script type to this component
  /// @tparam T Script type (must inherit from ScriptableEntity)
  template <typename T> void Bind() {
    InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
    DestroyScript = [](NativeScriptComponent* nsc) {
      delete nsc->instance;
      nsc->instance = nullptr;
    };
    scriptTypeName = typeid(T).name();
  }

  /// @brief Bind a script by its registered string name
  void Bind(const std::string& name) {
    if (instance) {
      instance->OnDestroy();
      if (DestroyScript) DestroyScript(this);
    }
    scriptTypeName = name;
    if (name.empty()) {
      InstantiateScript = nullptr;
      DestroyScript = nullptr;
    } else {
      InstantiateScript = [name]() { return ScriptRegistry::Instantiate(name); };
      DestroyScript = [](NativeScriptComponent* nsc) {
        delete nsc->instance;
        nsc->instance = nullptr;
      };
    }
  }

  /// @brief Set the scene reference for the script
  void SetScene(class Scene* scene) { scene_ = scene; }

  /// @brief Set the scene context reference for the script
  void SetSceneContext(SceneContext* ctx) { sceneContext_ = ctx; }

  void Update(float deltaTime) override {
    if (!instance && InstantiateScript) {
      instance = InstantiateScript();
      instance->entity_ = GetEntity();
      instance->scene_ = scene_;
      instance->sceneContext_ = sceneContext_;
      instance->OnCreate();
    }
    if (instance) {
      // Update context each frame (may change between frames)
      instance->scene_ = scene_;
      instance->sceneContext_ = sceneContext_;
      instance->OnUpdate(deltaTime);
    }
  }

  const char* TypeName() const override { return "NativeScriptComponent"; }

  nlohmann::json Serialize() const override {
    // Note: Due to lack of C++ reflection, we only save the script type name.
    // Restoring the correct type pointer automatically via JSON requires a factory registry for Scripts.
    // For now, this just saves the name.
    return {
      {"scriptTypeName", scriptTypeName}
    };
  }

  void Deserialize(const nlohmann::json& j) override {
    if (j.contains("scriptTypeName")) {
      std::string name = j["scriptTypeName"].get<std::string>();
      Bind(name);
    }
  }

private:
  class Scene* scene_ = nullptr;
  SceneContext* sceneContext_ = nullptr;
};
