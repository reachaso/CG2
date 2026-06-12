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
  std::shared_ptr<Entity> CreateEntity(const std::string& name) {
      auto e = std::make_shared<Entity>(name);
      entities_.push_back(e);
      return e;
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

  /// @brief GameModeの取得
  GameModeBase* GetGameMode() const { return gameMode_.get(); }

protected:
  std::vector<std::shared_ptr<Entity>> entities_; ///< シーン内のエンティティ一覧
  uint32_t selectedEntityId_ = 0; ///< 選択中のエンティティID
  std::unique_ptr<GameModeBase> gameMode_ = std::make_unique<GameModeBase>(); ///< ゲームルールの管理

  /// @brief Update all active entities
  void UpdateEntities(float deltaTime) {
    for (auto& e : entities_) {
      if (e && e->IsActive() && !e->IsPendingDestroy()) {
        e->UpdateAll(deltaTime);
      }
    }
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
};
