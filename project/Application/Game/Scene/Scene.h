#pragma once
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "AppConfig.h"
#include "ECS/Entity.h"
#include "../Framework/GameModeBase.h"

// 前方宣言（App 側の実体を参照するため）
class Dx12Core;
class Input;
class DebugCamera;
class MainCamera;
class ImGuiManager;
class PipelineManager;
class BgmManager;
class SeManager;
class PostProcess;
namespace RC { class CameraController; }

/// @enum PlayState
/// @brief ゲームの再生状態を表す列挙型
enum class PlayState {
  Stopped, // 停止中（編集モード）
  Playing, // 再生中
  Paused   // 一時停止中
};

/// @struct SceneContext
/// @brief シーン間で共有されるエンジンコンポーネントへの参照を保持する構造体
/// @details 各シーンの Update/Render に渡され、グラフィックスデバイス、入力、オーディオ、デバッグツールなどへのアクセスを提供します。
struct SceneContext {
  Dx12Core *core = nullptr;             ///< DirectX12 コアシステム
  Input *input = nullptr;               ///< 入力システム（キーボード、マウス、コントローラー）
  AppConfig *app = nullptr;             ///< アプリケーション設定
  ImGuiManager *imgui = nullptr;         ///< ImGui 管理
  PipelineManager *pipelineManager = nullptr; ///< パイプライン管理
  PostProcess *postProcess = nullptr;     ///< ポストプロセス管理
  BgmManager *bgmManager = nullptr;      ///< BGM 管理
  SeManager *seManager = nullptr;        ///< SE 管理
  RC::CameraController *camera = nullptr; ///< エディタカメラ（SceneManager が所有）
  float deltaTime = 1.0f / 60.0f;        ///< 前フレームからの経過時間 (秒)
  
  PlayState playState = PlayState::Playing; ///< 現在の再生状態

  /// @brief 再生中かどうか判定する
  bool isPlaying() const {
    return playState == PlayState::Playing;
  }
};

/// @class Scene
/// @brief ゲームシーンの基底抽象クラス
/// @details 全てのゲームシーン（タイトル、ゲーム本編、リザルトなど）はこのクラスを継承して実装します。
/// シーンのライフサイクル（入場・退場・更新・描画）を定義します。
class Scene {
public:
  virtual ~Scene() = default;

  /// @brief シーン名を取得する
  /// @return シーン名（文字列リテラル）
  virtual const char *Name() const = 0;

  /// @brief シーンに遷移した瞬間に呼び出される
  /// @param ctx シーンコンテキスト
  virtual void OnEnter(SceneContext &) {}

  /// @brief シーンから離れる瞬間に呼び出される
  /// @param ctx シーンコンテキスト
  virtual void OnExit(SceneContext &) {}

  class SceneManager; ///< 前方宣言

  /// @brief シーンの更新処理
  /// @param sm シーンマネージャー（シーン遷移要求に使用）
  /// @param ctx シーンコンテキスト
  virtual void Update(SceneManager &sm, SceneContext &ctx) = 0;

  /// @brief シーンの描画処理
  /// @param ctx シーンコンテキスト
  /// @param cl グラフィックスコマンドリスト
  virtual void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) = 0;

  /// @brief シーンに属するエンティティのリストを取得
  const std::vector<std::shared_ptr<Entity>>& GetEntities() const { return entities_; }

  /// @brief 新しいエンティティを生成してシーンに追加する
  /// @param name エンティティの名前
  /// @details UpdateEntities のループ中に呼び出しても安全。
  ///          エンティティは pendingEntities_ に追加され、FlushPendingEntities() で
  ///          entities_ にマージされる。
  std::shared_ptr<Entity> CreateEntity(const std::string& name) {
      auto e = std::make_shared<Entity>(name);
      pendingEntities_.push_back(e);
      return e;
  }

  /// @brief pendingEntities_ に溜まった新規エンティティを entities_ にマージする
  /// @details UpdateEntities のループ完了後に呼ぶこと
  void FlushPendingEntities() {
      if (!pendingEntities_.empty()) {
          entities_.insert(entities_.end(),
                           std::make_move_iterator(pendingEntities_.begin()),
                           std::make_move_iterator(pendingEntities_.end()));
          pendingEntities_.clear();
      }
  }

  /// @brief IDでエンティティを削除マーク（次フレームで除去）
  /// @param entityId エンティティのID
  void RemoveEntity(uint32_t entityId) {
      for (auto& e : entities_) {
          if (e && e->GetId() == entityId) {
              e->Destroy();
              break;
          }
      }
  }

  /// @brief 破棄マークされたエンティティを実際に除去する
  void CleanupDestroyedEntities() {
      entities_.erase(
          std::remove_if(entities_.begin(), entities_.end(),
              [this](const std::shared_ptr<Entity>& e) {
                  if (e && e->IsPendingDestroy()) {
                      ReleaseDynamicEntityRuntime(*e);
                      return true;
                  }
                  return false;
              }),
          entities_.end());
  }

  /// @brief 選択中のライトエンティティのギズモ（ワイヤフレーム）を描画する
  /// @param selectedEntityId 選択中のエンティティID（0なら描画しない）
  /// @details PreDraw3D ～ PreDraw2D の間で呼ぶ必要がある
  void DrawLightGizmos(uint32_t selectedEntityId = 0);

  /// @brief 選択中のカメラエンティティのFrustumギズモを描画する
  /// @param selectedEntityId 選択中のエンティティID（0なら描画しない）
  /// @param aspect アスペクト比
  void DrawCameraGizmos(uint32_t selectedEntityId = 0, float aspect = 16.0f / 9.0f);

  /// @brief 選択中のコライダーエンティティのギズモを描画する
  /// @param selectedEntityId 選択中のエンティティID（0なら描画しない）
  void DrawColliderGizmos(uint32_t selectedEntityId = 0);

  /// @brief 選択中のエンティティIDを設定する
  void SetSelectedEntityId(uint32_t id) { selectedEntityId_ = id; }
  /// @brief 選択中のエンティティIDを取得する
  uint32_t GetSelectedEntityId() const { return selectedEntityId_; }

  /// @brief 全コライダーのデバッグ描画フラグを設定する
  void SetShowColliderGizmos(bool show) { showColliderGizmos_ = show; }
  /// @brief 全コライダーのデバッグ描画フラグを取得する
  bool GetShowColliderGizmos() const { return showColliderGizmos_; }

  /// @brief GameModeの取得
  GameModeBase* GetGameMode() const { return gameMode_.get(); }

  /// @brief 現在のSceneContextを取得する
  SceneContext* GetContext() const { return currentContext_; }

  /// @brief 動的に生成したエンティティのランタイムリソースを初期化する
  /// @param e 初期化するエンティティ
  /// @details 派生クラスでオーバーライドして、モデルロードやメッシュ生成を行う
  virtual void InitDynamicEntityRuntime(Entity& e) {}

  /// @brief 動的に生成したエンティティのランタイムリソースを解放する
  /// @param e 解放するエンティティ
  virtual void ReleaseDynamicEntityRuntime(Entity& e) {}

protected:
  std::vector<std::shared_ptr<Entity>> entities_; ///< シーン内のエンティティ一覧
  std::vector<std::shared_ptr<Entity>> pendingEntities_; ///< 更新ループ中に生成されたエンティティの一時バッファ
  uint32_t selectedEntityId_ = 0; ///< 選択中のエンティティID
  std::unique_ptr<GameModeBase> gameMode_ = std::make_unique<GameModeBase>(); ///< ゲームルールの管理
  SceneContext* currentContext_ = nullptr; ///< 現在フレームのSceneContext
  bool showColliderGizmos_ = false; ///< コライダーを全描画するデバッグフラグ

  /// @brief Update all active entities
  /// @details インデックスベースループを使用し、ループ中の entities_ push_back による
  ///          イテレータ無効化を回避する（新規エンティティは pendingEntities_ 経由で追加される）
  void UpdateEntities(float deltaTime) {
    const size_t count = entities_.size(); // ループ前にサイズを固定
    for (size_t i = 0; i < count; ++i) {
      auto& e = entities_[i];
      if (e && e->IsActive() && !e->IsPendingDestroy()) {
        e->UpdateAll(deltaTime);
      }
    }
    FlushPendingEntities();
  }

  /// @brief Resolve physics collisions among entities
  void ResolveCollisions();

  /// @brief Remove entities marked for destruction
  void RemoveDeadEntities() {
    entities_.erase(
      std::remove_if(entities_.begin(), entities_.end(),
        [](const std::shared_ptr<Entity>& e) {
          return !e || e->IsPendingDestroy();
        }),
      entities_.end());
  }

  /// @brief Destroy entity by name
  /// @return true if found and marked
  bool DestroyEntityByName(const std::string& name) {
    for (auto& e : entities_) {
      if (e && e->Name() == name) { e->Destroy(); return true; }
    }
    return false;
  }

  /// @brief Destroy entity by ID
  /// @return true if found and marked
  bool DestroyEntityById(uint32_t id) {
    for (auto& e : entities_) {
      if (e && e->Id() == id) { e->Destroy(); return true; }
    }
    return false;
  }

  /// @brief Find entity by name
  /// @return Pointer to entity, or nullptr
  Entity* FindEntityByName(const std::string& name) {
    for (auto& e : entities_) {
      if (e && e->Name() == name) return e.get();
    }
    return nullptr;
  }

public:
  /// @brief Find entity by ID
  std::shared_ptr<Entity> FindEntityById(uint32_t id) {
    for (auto& e : entities_) {
      if (e && e->Id() == id) return e;
    }
    return nullptr;
  }

  /// @brief Find entity by GUID
  std::shared_ptr<Entity> FindEntityByGuid(uint64_t guid) {
    for (auto& e : entities_) {
      if (e && e->Guid() == guid) return e;
    }
    return nullptr;
  }

  /// @brief Destroy an entity and all its children recursively
  void DestroyEntityRecursive(std::shared_ptr<Entity> entity) {
    if (!entity) return;
    entity->Destroy();
    for (auto& child : entities_) {
      if (child && child->ParentGuid() == entity->Guid()) {
        DestroyEntityRecursive(child);
      }
    }
  }
};
