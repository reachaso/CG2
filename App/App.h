#pragma once
#include "AppConfig.h"
#include "Camera/CameraController.h"
#include "Dx12Core.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Input/Input.h"
#include "Log/Log.h"
#include "PipelineManager.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Sphere/Sphere.h"
#include "Texture/TextureManager/TextureManager.h"
#include "Window/Window.h"

#include <Windows.h>
#include <memory>

extern Log logger;

class TitleScene;
class SelectScene;
class GameScene;
class ResultScene;

class App {
public:
  App();
  ~App();

  bool Init();
  int Run();
  void Term();

private:
  void Update();
  void Render();

private:
  ID3D12GraphicsCommandList *cl = nullptr;
  ID3D12Device *device = nullptr;

  // 設定 & ログ
  AppConfig appConfig_;

  // Window / DX12
  std::unique_ptr<Window> window_;
  Dx12Core core_;
  Dx12Core::Desc coreDesc_{};

  // 入力
  std::unique_ptr<Input> input_;

  // ImGui
  ImGuiManager imgui_;

  // パイプライン
  PipelineManager pm_;
  GraphicsPipeline *pipeObj_ = nullptr;

  // === シーン管理 ===
  Scene::SceneManager sceneMgr_;
  SceneContext sceneCtx_;

  // Win32
  MSG msg_{};
};
