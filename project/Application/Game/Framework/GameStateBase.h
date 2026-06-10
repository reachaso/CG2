#pragma once

/// @class GameStateBase
/// @brief ゲームの進行状況、スコア、経過時間などを管理するクラス
/// @details Unreal Engineの AGameStateBase に相当。
class GameStateBase {
public:
    GameStateBase() = default;
    virtual ~GameStateBase() = default;

    /// @brief ゲーム開始時の初期化処理
    virtual void BeginPlay() {}

    /// @brief 毎フレームの更新処理
    /// @param deltaTime 経過時間
    virtual void Tick(float deltaTime) {
        elapsedTime_ += deltaTime;
    }

    /// @brief 経過時間の取得
    float GetElapsedTime() const { return elapsedTime_; }

    /// @brief スコアの加算
    void AddScore(int score) { score_ += score; }
    
    /// @brief 現在のスコアの取得
    int GetScore() const { return score_; }

protected:
    float elapsedTime_ = 0.0f;
    int score_ = 0;
};
