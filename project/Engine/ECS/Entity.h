#pragma once
#include "IComponent.h"
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

/// @brief 軽量エンティティクラス。コンポーネントを保持するための最小限のコンテナ。
/// テンプレートベースの AddComponent / GetComponent により型安全にコンポーネントを管理します。
class Entity {
public:
  /// @brief コンストラクタ。内部IDは自動で採番されます。
  Entity() : id_(NextId()) {}
  
  /// @brief 名前指定付きコンストラクタ
  /// @param name エンティティの名称
  explicit Entity(const std::string &name) : id_(NextId()), name_(name) {}

  /// @brief コンポーネントの追加
  /// @tparam T 追加するコンポーネントの型（IComponentを継承している必要があります）
  /// @tparam Args コンポーネントのコンストラクタ引数
  /// @param args 引数の実体
  /// @return 追加されたコンポーネントへの参照（既に同型があれば上書きされます）
  template <typename T, typename... Args> T &AddComponent(Args &&...args) {
    static_assert(std::is_base_of_v<IComponent, T>,
                  "T must derive from IComponent");
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    T &ref = *ptr;
    components_[std::type_index(typeid(T))] = std::move(ptr);
    return ref;
  }

  /// @brief コンポーネントの取得
  /// @tparam T 取得したいコンポーネントの型
  /// @return コンポーネントへのポインタ。存在しない場合は nullptr
  template <typename T> T *GetComponent() {
    auto it = components_.find(std::type_index(typeid(T)));
    if (it != components_.end()) {
      return static_cast<T *>(it->second.get());
    }
    return nullptr;
  }

  /// @brief コンポーネントの取得 (const)
  /// @tparam T 取得したいコンポーネントの型
  /// @return コンポーネントへのポインタ。存在しない場合は nullptr
  template <typename T> const T *GetComponent() const {
    auto it = components_.find(std::type_index(typeid(T)));
    if (it != components_.end()) {
      return static_cast<const T *>(it->second.get());
    }
    return nullptr;
  }

  /// @brief 指定した型のコンポーネントを保持しているか確認
  /// @tparam T 保持を確認したいコンポーネントの型
  /// @return 保持していれば true
  template <typename T> bool HasComponent() const {
    return components_.count(std::type_index(typeid(T))) > 0;
  }

  /// @brief 指定した型のコンポーネントを削除
  /// @tparam T 削除したいコンポーネントの型
  template <typename T> void RemoveComponent() {
    components_.erase(std::type_index(typeid(T)));
  }

  /// @brief 保持している全コンポーネントの更新処理を呼び出す
  /// @param deltaTime 前フレームからの経過時間
  void UpdateAll(float deltaTime) {
    for (auto &[type, comp] : components_) {
      if (comp && comp->IsEnabled()) {
        comp->Update(deltaTime);
      }
    }
  }

  /// @brief エンティティがアクティブか取得
  /// @return アクティブなら true
  bool IsActive() const { return active_; }
  
  /// @brief エンティティのアクティブ状態を設定
  /// @param active 設定する状態
  void SetActive(bool active) { active_ = active; }

  /// @brief ユニークなIDを取得
  /// @return エンティティID
  uint32_t Id() const { return id_; }
  
  /// @brief エンティティ名を取得
  /// @return 名前文字列
  const std::string &Name() const { return name_; }
  
  /// @brief エンティティ名を設定
  /// @param name 設定する名前
  void SetName(const std::string &name) { name_ = name; }

  /// @brief エンティティが可視か取得
  bool IsVisible() const { return visible_; }
  /// @brief エンティティの可視状態を設定
  void SetVisible(bool v) { visible_ = v; }

  /// @brief エンティティがロックされているか取得
  bool IsLocked() const { return locked_; }
  /// @brief エンティティのロック状態を設定
  void SetLocked(bool l) { locked_ = l; }

private:
  uint32_t id_; ///< エンティティ固有のID
  std::string name_; ///< 識別用の名前
  bool active_ = true; ///< アクティブフラグ
  bool visible_ = true; ///< 可視フラグ（Hierarchy制御用）
  bool locked_ = false; ///< ロックフラグ（Hierarchy制御用）

  /// 型インデックスをキーとしたコンポーネントのマップ
  std::unordered_map<std::type_index, std::unique_ptr<IComponent>> components_;

  /// @brief 次のユニークIDを生成する
  /// @return 生成されたID
  static uint32_t NextId() {
    static uint32_t counter = 0;
    return ++counter;
  }
};
