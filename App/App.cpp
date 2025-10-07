#include "App.h"
#include <cassert>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

Log logger;

App::App(const AppConfig &cfg) : appCfg_(cfg) {}
App::~App() { /* Run() 経由で確実に term_() する */ }

void App::init_() {
  // Window
  window_.Initialize(appCfg_.title.c_str(), appCfg_.width, appCfg_.height);
  window_.SetClearColor(appCfg_.clearColor[0], appCfg_.clearColor[1],
                        appCfg_.clearColor[2], appCfg_.clearColor[3]);

  // Dx12Core 設定
  cd_.width = appCfg_.width;
  cd_.height = appCfg_.height;
  core_.Init(window_.hwnd, cd_);

  // 入力
  input_ = std::make_unique<Input>(window_.hwnd);

  // ShaderCompiler
  assert(shaderCompiler_.Init());
  vs_ =
      shaderCompiler_.Compile({L"Shader/Object3D.VS.hlsl", L"vs_6_0", L"main"});
  ps_ =
      shaderCompiler_.Compile({L"Shader/Object3D.PS.hlsl", L"ps_6_0", L"main"});
  assert(vs_.HasBlob() && ps_.HasBlob());

  // GraphicsPipeline（main.cppの Build 呼び出しを踏襲）
  // 入力レイアウト
  D3D12_INPUT_ELEMENT_DESC inputElems[3] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
  pipeline_.Init(core_.GetDevice());
  pipeline_.Build(inputElems, _countof(inputElems), vs_.Bytecode(),
                  ps_.Bytecode(), cd_.rtvFormat, cd_.dsvFormat);

  // カメラ
  debugCamera_.Initialize(input_.get(), 0.45f,
                          float(appCfg_.width) / float(appCfg_.height), 0.1f,
                          100.0f);

  mainCamera_.Initialize({0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, 0.45f,
                         float(appCfg_.width) / float(appCfg_.height), 0.1f,
                         100.0f);

  // Viewport / Scissor
  viewport_.Width = static_cast<float>(appCfg_.width);
  viewport_.Height = static_cast<float>(appCfg_.height);
  viewport_.TopLeftX = 0.0f;
  viewport_.TopLeftY = 0.0f;
  viewport_.MinDepth = 0.0f;
  viewport_.MaxDepth = 1.0f;

  scissorRect_.left = 0;
  scissorRect_.top = 0;
  scissorRect_.right = appCfg_.width;
  scissorRect_.bottom = appCfg_.height;

  // ImGui
  imgui_.Init(window_.hwnd, core_);

  // ===== Scene 初期化 =====
  sceneCtx_.core = &core_;
  sceneCtx_.input = input_.get();
  sceneCtx_.dbgCam = &debugCamera_;
  sceneCtx_.mainCam = &mainCamera_;
  sceneCtx_.imgui = &imgui_;

  sceneMgr_.Register(std::make_unique<TitleScene>());
  sceneMgr_.Register(std::make_unique<SelectScene>());
  sceneMgr_.Register(std::make_unique<GameScene>());
  sceneMgr_.Register(std::make_unique<ResultScene>());
  sceneMgr_.ChangeImmediately("Title", sceneCtx_);

#ifdef _DEBUG

  sceneMgr_.ChangeImmediately("Game", sceneCtx_);

#endif // DEBUG
}

void App::term_() {
  imgui_.Shutdown();
  pipeline_.Term();
  shaderCompiler_.Term();
  core_.Term();
}

void App::frame_() {
  // ImGui
  imgui_.NewFrame();

  ImGui::Begin("カメラモード : Tab");
  if (useDebugCamera_) {
    ImGui::Text("デバッグカメラモード");
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    if (ImGui::Button("リセット")) {
      debugCamera_.Reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("操作方法")) {
      isGuide_ = !isGuide_;
    }
  } else {
    ImGui::Text("メインカメラモード");
  }
  ImGui::End();

  if (isGuide_) {
    ImGui::Begin("操作方法", &isGuide_, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text(" WASD : 前後左右移動");
    ImGui::Text(" QE : 上下移動");
    ImGui::Text(" 十字キー : カメラの回転");
    ImGui::Text(" マウスホイール : 前後移動");
    ImGui::Text(" 左クリックしながらドラッグ : 回転");
    ImGui::Text(" マウスホイール押しながらドラッグ : 上下左右移動");
    ImGui::End();
  }

  // 入力更新／カメラ切替
  input_->Update();
  if (input_->IsKeyTrigger(DIK_TAB)) {
    useDebugCamera_ = !useDebugCamera_;
  }
  if (useDebugCamera_) {
    debugCamera_.Update();
  } else {
    mainCamera_.Update();
  }

  const Matrix4x4 &view =
      useDebugCamera_ ? debugCamera_.GetView() : mainCamera_.GetView();
  const Matrix4x4 &proj = useDebugCamera_ ? debugCamera_.GetProjection()
                                          : mainCamera_.GetProjection();
  (void)view;
  (void)proj; // まだ使ってないので警告抑止

  // === シーン更新 ===
  sceneMgr_.Update(sceneCtx_);

  // ====== DX12 コマンド列 ======
  core_.BeginFrame();
  ID3D12GraphicsCommandList *commandList = core_.CL();

  ID3D12DescriptorHeap *heaps[] = {core_.SRV().Heap()};
  commandList->SetDescriptorHeaps(1, heaps);

  // クリア（AppConfigに同期したWindowとは別に、Dx12側のRT/DSもクリア）
  core_.Clear(appCfg_.clearColor[0], appCfg_.clearColor[1],
              appCfg_.clearColor[2], appCfg_.clearColor[3]);

  // Viewport / Scissor
  commandList->RSSetViewports(1, &viewport_);
  commandList->RSSetScissorRects(1, &scissorRect_);

  // PSO 等セット
  commandList->SetGraphicsRootSignature(pipeline_.Root());
  commandList->SetPipelineState(pipeline_.PSO());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // === シーン描画 ===
  sceneMgr_.Render(sceneCtx_, commandList);

  // ImGui 描画
  imgui_.Render(commandList);

  core_.EndFrame();
}

int App::Run() {
  init_();

  // ループ
  while (msg_.message != WM_QUIT) {
    if (PeekMessage(&msg_, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg_);
      DispatchMessage(&msg_);
    } else {
      frame_();
    }
  }

  term_();
  return 0;
}
