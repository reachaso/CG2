#pragma once
#include <memory>
#include "GameStateBase.h"

// 前方宣言
struct SceneContext;

/// @class GameModeBase
/// @brief ゲームのルール、勝利条件、進行を管理するクラス
/// @details Unreal Engineの AGameModeBase に相当。
class GameModeBase {
public:
    GameModeBase();
    virtual ~GameModeBase() = default;

    /// @brief ゲーム開始時の初期化処理
    /// @param ctx シーンコンテキスト
    virtual void BeginPlay(SceneContext& ctx);

    /// @brief 毎フレームの更新処理
    /// @param ctx シーンコンテキスト
    virtual void Tick(SceneContext& ctx);

    /// @brief GameStateの取得
    GameStateBase* GetGameState() const { return gameState_.get(); }

    bool HasBegunPlay() const { return hasBegunPlay_; }
    void MarkBegunPlay() { hasBegunPlay_ = true; }

    /// @brief シーンを再入場したときに BeginPlay からやり直せる状態へ戻す
    /// @details hasBegunPlay_ は一度立つと二度と下りないため、GameMode を作り直さずに
    ///          使い回すと、リトライしても BeginPlay が呼ばれずスコアと経過時間が
    ///          前回のまま積み上がってしまう。シーンの OnEnter から呼ぶこと。
    virtual void ResetForRestart() {
        hasBegunPlay_ = false;
        if (gameState_) {
            gameState_->Reset();
        }
    }

    /// @brief カスタムのGameStateを設定（派生クラスのコンストラクタ等で利用）
    template <typename T>
    void SetGameState() {
        gameState_ = std::make_unique<T>();
    }

protected:
    std::unique_ptr<GameStateBase> gameState_;
    bool hasBegunPlay_ = false;
};
