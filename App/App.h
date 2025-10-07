#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <string>

#include "AppConfig.h"
#include "DebugCamera/DebugCamera.h"
#include "Dx12Core.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Input/Input.h"
#include "Log/Log.h"
#include "MainCamera/MainCamera.h"
#include "Math/Math.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Window/Window.h"
#include "function/function.h"
#include "imgui/imgui.h"
#include "struct.h"
#include <memory>

#include "SceneManager.h"
#include "TitleScene/TitleScene.h"
#include "SelectScene/SelectScene.h"
#include "GameScene/GameScene.h"
#include "ResultScene/ResultScene.h"

extern Log logger;

class App {
public:
  explicit App(const AppConfig &cfg);
  ~App();

  // 初期化～メインループ～終了までをまとめて実行
  int Run();

private:
  void init_();  // 初期化（ウィンドウ、DX12、各モジュール）
  void term_();  // 解放
  void frame_(); // 1フレーム

private:
  AppConfig appCfg_;

  // main.cppのローカル変数をそのままメンバ化
  Window window_;
  Dx12Core core_;
  Dx12Core::Desc cd_{};

  std::unique_ptr<Input> input_;
  ShaderCompiler shaderCompiler_;
  GraphicsPipeline pipeline_;
  DebugCamera debugCamera_;
  MainCamera mainCamera_;
  ImGuiManager imgui_;

  // 状態フラグ
  bool useDebugCamera_ = false;
  bool isGuide_ = false;

  // ビューポート／シザー（main.cpp相当）
  D3D12_VIEWPORT viewport_{};
  D3D12_RECT scissorRect_{};

  // シェーダ
  CompiledShader vs_{}, ps_{};

  // メッセージ
  MSG msg_{};

  // シーン管理
  SceneContext sceneCtx_{};
  Scene::SceneManager sceneMgr_;
};
