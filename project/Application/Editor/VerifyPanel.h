#pragma once

// ============================================================================
// VerifyPanel — 実装確認パネル
// ----------------------------------------------------------------------------
// 直近に実装した機能を「ゲームを動かしたまま」その場で確かめるための ImGui パネル。
//
// なぜ作ったか:
//   このプロジェクトの検証は録画を撮って後から見返す形だったため、
//   「反射が見えないのはシェーダのせいか、シーン設定のせいか」を切り分けるのに
//   毎回ビルドし直す必要があった。値をその場で動かして見比べられれば、
//   原因の切り分けが1回のプレイの中で終わる。
//
// 設計上の制約（重要）:
//   ゲームプレイのスクリプト（RailMovementScript / BuoyancyScript /
//   WaveManagerScript など）はすべて .cpp 内でクラス定義されており、
//   ヘッダが無い。したがって別の翻訳単位であるこのファイルからは
//   dynamic_cast も直接のメンバアクセスもできない。
//   代わりに次の3つだけを使う:
//     1. GameSession        … 唯一ヘッダを持つシングルトン。読み書き自由
//     2. ScriptableEntity   … 基底の仮想関数（OnImGui / Serialize）だけ呼ぶ
//     3. Entity のタグ辞書  … スクリプト同士が元々これで通信しているので、
//                             同じ口を使えば型を知らなくても状態を読み書きできる
//
//   スクリプトを ヘッダへ切り出せば直接呼べるようになるが、
//   確認のために本体の構成を変えるのは本末転倒なので採らない。
// ============================================================================

#include <functional>
#include <string>

class Dx12Core;
class Scene;

/// @class VerifyPanel
/// @brief 直近の実装をゲーム内で確認するためのパネル
/// @details 状態は静的に持つ（EditorManager のメンバを増やさずに済ませるため）。
///          パネルは1つしか出さないので実害はない。
class VerifyPanel {
public:
  /// @brief パネルを描画する
  /// @param open 表示フラグ（ウィンドウの × で false になる）
  /// @param currentScene 現在のシーン。nullptr 可
  /// @param core Dx12Core。シャドウマップのプレビューに使う。nullptr 可
  static void Draw(bool *open, Scene *currentScene, Dx12Core *core);

  /// @brief シーン切り替えの依頼先を登録する
  /// @details Scene からは SceneManager へ辿れないため、App 側から
  ///          Game::RequestChange を包んだ関数を1度だけ渡してもらう。
  ///          未登録のあいだシーン切り替えボタンは無効表示になる。
  static void SetSceneRequest(std::function<void(const std::string &)> fn);

  /// @brief チェックリストの状態をファイルへ保存する
  static void SaveChecklist();

  /// @brief チェックリストの状態をファイルから読み込む
  static void LoadChecklist();
};
