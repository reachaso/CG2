#pragma once
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "AppConfig.h"

// 前方宣言（App 側の実体を参照するため）
class Dx12Core;
class Input;
class DebugCamera;
class MainCamera;
class ImGuiManager;

// シーンが使う共有コンテキスト
struct SceneContext {
  Dx12Core *core = nullptr;      // コマンドリスト等のアクセスに使う場合
  Input *input = nullptr;        // 入力（キー遷移など）
  const AppConfig *app = nullptr;
  ImGuiManager *imgui =
      nullptr; // ImGuiウィンドウを出すだけなら不要だが念のため
};

// シーン基底クラス
class Scene {
public:
  virtual ~Scene() = default;
  virtual const char *Name() const = 0;

  // シーン遷移の入り口／出口
  virtual void OnEnter(SceneContext &) {}
  virtual void OnExit(SceneContext &) {}

  // 1フレーム更新／描画
  // Update 内で SceneManager に遷移要求を出せるよう、後述の SceneManager
  // を前方宣言で受ける
  class SceneManager;
  virtual void Update(SceneManager &sm, SceneContext &ctx) = 0;
  virtual void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) = 0;
};
