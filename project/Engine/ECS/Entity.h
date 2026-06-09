#pragma once
#include "IComponent.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <nlohmann/json.hpp>

/// @brief Lightweight entity class. Minimal container for holding components.
/// Uses template-based AddComponent / GetComponent for type-safe component management.
class Entity {
public:
  /// @brief Constructor. Internal ID is auto-assigned.
  Entity() : id_(NextId()) {}
  
  /// @brief Named constructor
  /// @param name Entity name
  explicit Entity(const std::string &name) : id_(NextId()), name_(name) {}

  /// @brief Add a component
  /// @tparam T Component type (must derive from IComponent)
  /// @tparam Args Constructor arguments
  /// @param args Arguments
  /// @return Reference to the added component (overwrites if same type exists)
  template <typename T, typename... Args> T &AddComponent(Args &&...args) {
    static_assert(std::is_base_of_v<IComponent, T>,
                  "T must derive from IComponent");
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    ptr->entity_ = this;
    T &ref = *ptr;
    components_[std::type_index(typeid(T))] = std::move(ptr);
    return ref;
  }

  /// @brief Get a component
  /// @tparam T Component type to get
  /// @return Pointer to the component, or nullptr if not found
  template <typename T> T *GetComponent() {
    auto it = components_.find(std::type_index(typeid(T)));
    if (it != components_.end()) {
      return static_cast<T *>(it->second.get());
    }
    return nullptr;
  }

  /// @brief Get a component (const)
  /// @tparam T Component type to get
  /// @return Pointer to the component, or nullptr if not found
  template <typename T> const T *GetComponent() const {
    auto it = components_.find(std::type_index(typeid(T)));
    if (it != components_.end()) {
      return static_cast<const T *>(it->second.get());
    }
    return nullptr;
  }

  /// @brief Check if entity has a component of given type
  /// @tparam T Component type to check
  /// @return true if the component exists
  template <typename T> bool HasComponent() const {
    return components_.count(std::type_index(typeid(T))) > 0;
  }

  /// @brief Remove a component of given type
  /// @tparam T Component type to remove
  template <typename T> void RemoveComponent() {
    components_.erase(std::type_index(typeid(T)));
  }

  /// @brief Update all components
  /// @param deltaTime Delta time since last frame
  void UpdateAll(float deltaTime) {
    for (auto &[type, comp] : components_) {
      if (comp && comp->IsEnabled()) {
        comp->Update(deltaTime);
      }
    }
  }

  /// @brief Get active state
  /// @return true if active
  bool IsActive() const { return active_; }
  
  /// @brief Set active state
  /// @param active State to set
  void SetActive(bool active) { active_ = active; }

  /// @brief Mark entity for deferred destruction
  /// @details Not immediately deleted; collected by Scene::RemoveDeadEntities()
  void Destroy() { pendingDestroy_ = true; }

  /// @brief Check if entity is pending destruction
  /// @return true if marked for destruction
  bool IsPendingDestroy() const { return pendingDestroy_; }

  /// @brief Get unique ID
  /// @return Entity ID
  uint32_t Id() const { return id_; }
  
  /// @brief Get entity name
  /// @return Name string
  const std::string &Name() const { return name_; }
  
  /// @brief Set entity name
  /// @param name Name to set
  void SetName(const std::string &name) { name_ = name; }

  /// @brief Get visibility
  bool IsVisible() const { return visible_; }
  /// @brief Set visibility
  void SetVisible(bool v) { visible_ = v; }

  /// @brief Get lock state
  bool IsLocked() const { return locked_; }
  /// @brief Set lock state
  void SetLocked(bool l) { locked_ = l; }

  /// @brief Serialize entity state to JSON
  nlohmann::json Serialize() const {
    nlohmann::json j;
    j["name"] = name_;
    j["active"] = active_;
    j["visible"] = visible_;
    j["locked"] = locked_;
    nlohmann::json comps = nlohmann::json::array();
    for (auto& [type, comp] : components_) {
      if (comp) {
        nlohmann::json c;
        c["type"] = comp->TypeName();
        c["data"] = comp->Serialize();
        c["enabled"] = comp->IsEnabled();
        comps.push_back(c);
      }
    }
    j["components"] = comps;
    return j;
  }

  /// @brief Deserialize entity state from JSON
  /// @param j JSON object to deserialize from
  void Deserialize(const nlohmann::json& j) {
    if (j.contains("name")) name_ = j["name"].get<std::string>();
    if (j.contains("active")) active_ = j["active"].get<bool>();
    if (j.contains("visible")) visible_ = j["visible"].get<bool>();
    if (j.contains("locked")) locked_ = j["locked"].get<bool>();
    if (j.contains("components")) {
      for (auto& cj : j["components"]) {
        std::string typeName = cj["type"].get<std::string>();
        auto comp = ComponentFactory::Create(typeName);
        if (comp) {
          comp->entity_ = this;
          comp->Deserialize(cj["data"]);
          if (cj.contains("enabled")) comp->SetEnabled(cj["enabled"].get<bool>());
          components_[std::type_index(typeid(*comp))] = std::move(comp);
        }
      }
    }
  }

  // =================================================================
  // Component Factory (type name string -> creator function registry)
  // =================================================================
  class ComponentFactory {
  public:
    using Creator = std::function<std::unique_ptr<IComponent>()>;

    /// @brief Register a component creator function
    template <typename T>
    static void Register(const std::string& name) {
      Registry()[name] = []() { return std::make_unique<T>(); };
    }

    /// @brief Create a component from type name
    static std::unique_ptr<IComponent> Create(const std::string& name) {
      auto& reg = Registry();
      auto it = reg.find(name);
      if (it != reg.end()) return it->second();
      return nullptr;
    }

  private:
    static std::unordered_map<std::string, Creator>& Registry() {
      static std::unordered_map<std::string, Creator> registry;
      return registry;
    }
  };

private:
  uint32_t id_;
  std::string name_;
  bool active_ = true;
  bool visible_ = true;
  bool locked_ = false;
  bool pendingDestroy_ = false;

  std::unordered_map<std::type_index, std::unique_ptr<IComponent>> components_;

  /// @brief Generate next unique ID
  /// @return Generated ID
  static uint32_t NextId() {
    static uint32_t counter = 0;
    return ++counter;
  }
};
