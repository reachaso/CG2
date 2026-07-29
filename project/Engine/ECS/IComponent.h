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

  /// @brief Called just before the owning Entity tears down its component map
  /// @details エンティティが破棄されるとき、components_ の破棄が始まる前に全コンポーネントへ
  ///          通知される。他のコンポーネントを参照する後始末（スクリプトの OnDestroy など）は
  ///          デストラクタではなくここで行うこと。デストラクタから GetComponent() を呼ぶと、
  ///          破棄途中の unordered_map を検索することになり未定義動作になる。
  /// @note 同じインスタンスに対して複数回呼ばれても壊れないように実装すること
  virtual void OnEntityDestroy() {}

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

  /// @brief Get the entity this component is attached to
  /// @return Pointer to the parent Entity
  class Entity* GetEntity() const { return entity_; }

protected:
  bool enabled_ = true; ///< Enabled flag (Update skipped if false)
  class Entity* entity_ = nullptr; ///< Pointer to parent Entity
  friend class Entity;
};
