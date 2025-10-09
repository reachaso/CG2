#include "App.h"
#include "GameScene/GameScene.h"
#include "ResultScene/ResultScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"
#include "imgui/imgui.h"
#include <cassert>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

Log logger;

App::App() {}
App::~App() { Term(); }

bool App::Init() {
  // ===== Window =====
  window_ = std::make_unique<Window>();
  window_->Initialize(appConfig_.title.c_str(), appConfig_.width,
                      appConfig_.height);

  // ===== DX12 Core =====
  coreDesc_.width = appConfig_.width;
  coreDesc_.height = appConfig_.height;
  core_.Init(window_->GetHwnd(), coreDesc_);

  cl = core_.CL();
  device = core_.GetDevice();
  assert(device);

  // ===== Input =====
  input_ = std::make_unique<Input>(window_->GetHwnd());

  // ===== ImGui =====
  imgui_.Init(window_->GetHwnd(), core_);

  // ===== PipelineManager =====
  pm_.Init(device, coreDesc_.rtvFormat, coreDesc_.dsvFormat);

  // GraphicsPipeline一括構築（既存のObject3D）
  pipeObj_ = pm_.CreateFromFiles("object3d", L"Shader/Object3D.VS.hlsl",
                                 L"Shader/Object3D.PS.hlsl",
                                 InputLayoutType::Object3D);

  // ===== SceneContext の紐づけ =====
  sceneCtx_.core = &core_;
  sceneCtx_.input = input_.get();
  sceneCtx_.app = &appConfig_;
  sceneCtx_.imgui = &imgui_;

  // ===== シーン登録 =====
  sceneMgr_.Register(std::make_unique<TitleScene>());
  sceneMgr_.Register(std::make_unique<SelectScene>());
  sceneMgr_.Register(std::make_unique<GameScene>());
  sceneMgr_.Register(std::make_unique<ResultScene>());

  // ===== 最初のシーンへ即時遷移 =====
#ifdef _DEBUG
  sceneMgr_.ChangeImmediately("Game", sceneCtx_);
#else
  sceneMgr_.ChangeImmediately("Title", sceneCtx_);
#endif // _DEBUG
  return true;
}

int App::Run() {
  // メッセージループ
  while (msg_.message != WM_QUIT) {
    if (PeekMessage(&msg_, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg_);
      DispatchMessage(&msg_);
    } else {

      imgui_.NewFrame();
      Update();
      core_.BeginFrame();
      Render();
      imgui_.Render(cl);
      core_.EndFrame();
    }
  }
  return static_cast<int>(msg_.wParam);
}

void App::Update() {

  ImGui::Begin("Scene");
  ImGui::Text("Current: %s", sceneMgr_.CurrentName().c_str());
  if (ImGui::Button("Go Title"))
    sceneMgr_.RequestChange("Title");
  ImGui::SameLine();
  if (ImGui::Button("Go Select"))
    sceneMgr_.RequestChange("Select");
  ImGui::SameLine();
  if (ImGui::Button("Go Game"))
    sceneMgr_.RequestChange("Game");
  ImGui::SameLine();
  if (ImGui::Button("Go Result"))
    sceneMgr_.RequestChange("Result");
  ImGui::End();

  input_->Update();

  sceneMgr_.Update(sceneCtx_);
}

void App::Render() {

  // ルートシグネチャ/PSO/トポロジ設定
  cl->SetGraphicsRootSignature(pipeObj_->Root());
  cl->SetPipelineState(pipeObj_->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  sceneMgr_.Render(sceneCtx_, cl);
}

void App::Term() {
  pm_.Term();
  imgui_.Shutdown();

  core_.Term();

  if (input_) {
    input_.reset();
  }
  if (window_) {
    window_.reset();
  }
}
