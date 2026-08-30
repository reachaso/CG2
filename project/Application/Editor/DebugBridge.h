#pragma once

// ============================================================================
// DebugBridge — 確認用のUIから「アプリ側にしかできないこと」を頼むための細い口
// ----------------------------------------------------------------------------
// シーンの切り替えは SceneManager が持っているが、Scene からは SceneManager へ
// 辿れない（Update の引数として渡ってくるだけ）。一方 EditorManager の
// DrawUI に来るのは Scene* だけなので、確認用パネルからシーンを飛ばせない。
//
// そこで App が起動時に一度だけ Game::RequestChange を包んだ関数を置いていき、
// 確認用のUIはここを経由して依頼する。
// 未登録のあいだは RequestScene が false を返すので、UI 側はボタンを
// 無効表示にすればよい（つまり、この仕組みが無くても壊れない）。
//
// ヘッダオンリーにしてあるのは、実体が関数ローカル static 1つで足り、
// .cpp を足すと vcxproj の登録が増えるだけだから。
// inline 関数の関数ローカル static は翻訳単位をまたいで1つになることが
// 言語仕様で保証されている。
// ============================================================================

#include <functional>
#include <string>
#include <utility>

namespace DebugBridge {

/// @brief シーン切り替えを依頼する関数の置き場
inline std::function<void(const std::string &)> &SceneRequestFn() {
  static std::function<void(const std::string &)> fn;
  return fn;
}

/// @brief シーン切り替えの依頼先を登録する（App が起動時に一度だけ呼ぶ）
inline void SetSceneRequest(std::function<void(const std::string &)> fn) {
  SceneRequestFn() = std::move(fn);
}

/// @brief 依頼先が登録されているか
inline bool CanRequestScene() { return static_cast<bool>(SceneRequestFn()); }

/// @brief シーンの切り替えを依頼する
/// @return 依頼先が未登録なら false（何もしない）
inline bool RequestScene(const std::string &name) {
  auto &fn = SceneRequestFn();
  if (!fn) return false;
  fn(name);
  return true;
}

} // namespace DebugBridge
