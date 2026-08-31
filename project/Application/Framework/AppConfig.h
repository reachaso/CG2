#pragma once
#include <array>
#include <string>
#include "EngineConfig.h"

/// @struct AppConfig
/// @brief アプリケーションの起動設定を保持する構造体
/// @details 画面解像度、ウィンドウタイトル、V-Sync の有効無効、および画面クリアカラーを定義します。
struct AppConfig {
#if defined(RC_DEVELOPMENT)
  int width = 1280;  ///< ウィンドウの幅
  int height = 720; ///< ウィンドウの高さ
  bool fullscreen = false; ///< フルスクリーン表示にするか
#elif defined(_DEBUG)
  int width = 1920;  ///< ウィンドウの幅
  int height = 1080; ///< ウィンドウの高さ
  bool fullscreen = false; ///< フルスクリーン表示にするか
#else
  int width = 1280; ///< ウィンドウの幅
  int height = 720; ///< ウィンドウの高さ
  bool fullscreen = false; ///< フルスクリーン表示にするか
#endif
  bool vsync = true; ///< 垂直同期 (V-Sync) を有効にするか

#if defined(RC_DEVELOPMENT)
  std::string title = "LE3B_03_オオシマ_タイガ_CG4"; ///< 開発ビルド時のタイトル
#elif defined(_DEBUG)
  std::string title = "ChasoEngine"; ///< デバッグビルド時のタイトル
#else
  std::string title = "LE3B_03_オオシマ_タイガ_"; ///< リリースビルド時のタイトル
#endif

  std::array<float, 4> clearColor{0.1f, 0.25f, 0.5f, 1.0f}; ///< 画面のクリアカラー (RGBA)
};
