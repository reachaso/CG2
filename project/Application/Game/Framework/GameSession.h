#pragma once

/// @class GameSession
/// @brief シーンをまたいで 1 プレイ分の結果を保持する
/// @details Scene が持つ GameMode / GameState はシーンごとのインスタンスなので、
///          Game シーンで積んだスコアを Result シーンから読むことができない。
///          そこでプレイ結果だけをここに集約し、Result / GameOver から参照する。
///
///          値の流れ:
///            Game シーン OnEnter    -> BeginRun()   （前回のプレイ結果を捨てる）
///            プレイ中               -> RailShooterController が score / hp を書き込む
///            クリア or 死亡         -> Finish(Outcome) をシーン側が呼ぶ
///            Result / GameOver      -> Score() 等を読んで表示する
class GameSession {
public:
  /// @brief 1 プレイの決着のつき方
  enum class Outcome {
    InProgress, ///< プレイ中（まだ決着していない）
    Cleared,    ///< 敵を全滅させてクリアした
    GameOver    ///< プレイヤーが力尽きた
  };

  /// @brief 唯一のインスタンスを取得する
  static GameSession &Get() {
    static GameSession instance;
    return instance;
  }

  /// @brief 新しいプレイを開始する（Game シーンの OnEnter で呼ぶ）
  /// @details リトライのたびにスコアと経過時間をゼロへ戻す。
  ///          これを呼ばないと 2 周目以降にスコアが加算され続ける。
  void BeginRun() {
    score_ = 0;
    playerHp_ = 0;
    playerMaxHp_ = 0;
    elapsedTime_ = 0.0f;
    outcome_ = Outcome::InProgress;
    ++playCount_;
  }

  /// @brief プレイの決着を記録する
  void Finish(Outcome outcome) { outcome_ = outcome; }

  // --- プレイ中に書き込む値 ---
  void SetScore(int score) { score_ = score; }
  void SetPlayerHp(int hp, int maxHp) {
    playerHp_ = hp;
    playerMaxHp_ = maxHp;
  }
  void SetElapsedTime(float seconds) { elapsedTime_ = seconds; }

  // --- Result / GameOver から読む値 ---
  int Score() const { return score_; }
  int PlayerHp() const { return playerHp_; }
  int PlayerMaxHp() const { return playerMaxHp_; }
  float ElapsedTime() const { return elapsedTime_; }
  Outcome GetOutcome() const { return outcome_; }
  bool IsCleared() const { return outcome_ == Outcome::Cleared; }

  /// @brief 何回プレイしたか（1 周目なら 1）
  int PlayCount() const { return playCount_; }

  /// @brief クリア時の評価ランク（'S' / 'A' / 'B' / 'C'）
  /// @details スコアと残 HP から算出する簡易評価。
  ///          ゲームオーバー時は常に 'C' を返す。
  char Rank() const {
    if (outcome_ != Outcome::Cleared) return 'C';
    const float hpRate =
        (playerMaxHp_ > 0) ? static_cast<float>(playerHp_) / playerMaxHp_ : 0.0f;
    if (hpRate >= 0.99f) return 'S';
    if (hpRate >= 0.6f) return 'A';
    if (hpRate >= 0.3f) return 'B';
    return 'C';
  }

private:
  GameSession() = default;

  int score_ = 0;
  int playerHp_ = 0;
  int playerMaxHp_ = 0;
  float elapsedTime_ = 0.0f;
  Outcome outcome_ = Outcome::InProgress;
  int playCount_ = 0;
};
