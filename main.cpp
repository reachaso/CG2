#pragma warning(push)
#pragma warning(disable : 28251)
#include "AppConfig.h"
#include "Camera/CameraController.h"
#include "Dx12Core.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Input/Input.h"
#include "Log/Log.h"
#include "Math/Math.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Sphere/Sphere.h"
#include "Texture/TextureManager/TextureManager.h"
#include "Window/Window.h"
#include "imgui/imgui.h"
#include "struct.h"
#include <Windows.h>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma warning(pop)

Log logger;

AppConfig appConfig;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

  // COMライブラリの初期化に失敗したらエラー
  HRESULT hrCoInt = CoInitializeEx(0, COINIT_MULTITHREADED);

  // ===========================
  // winodwの初期化
  // ===========================
  Window window;
  window.Initialize(appConfig.title.c_str(), appConfig.width, appConfig.height);

  // ===========================
  // DX12の初期化
  // ===========================
  Dx12Core core;
  Dx12Core::Desc cd{};
  cd.width = appConfig.width;
  cd.height = appConfig.height;
  core.Init(window.GetHwnd(), cd);

  // よく使うハンドル
  ID3D12Device *device = core.GetDevice();

  // ===========================
  // Inputの初期化
  // ===========================
  Input input = Input(window.GetHwnd());

  //===============================
  // Imguiの初期化
  //===============================

  ImGuiManager imgui;
  imgui.Init(window.GetHwnd(), core);

  // ==========================
  // メッセージループ
  // ==========================
  MSG msg{};

  //==========================
  // Shaderをコンパイルする
  //==========================

  ShaderCompiler shaderCompiler;
  bool ok = shaderCompiler.Init();
  assert(ok && "ShaderCompiler::Init() failed");

  // VS
  ShaderDesc vs{};
  vs.path = L"Shader/Object3D.VS.hlsl";
  vs.target = L"vs_6_0";
  vs.entry = L"main";
  vs.optimize = true; // リリース時: true
  vs.debugInfo = false;
#ifdef _DEBUG
  vs.optimize = false; // デバッグ時: false
  vs.debugInfo = true;
#endif

  CompiledShader VS = shaderCompiler.Compile(vs);
  assert(VS.HasBlob() && "VS compile failed");

  // PS
  ShaderDesc ps{};
  ps.path = L"Shader/Object3D.PS.hlsl";
  ps.target = L"ps_6_0";
  ps.entry = L"main";
  ps.optimize = true;
  ps.debugInfo = false;
#ifdef _DEBUG
  ps.optimize = false; // デバッグ時: false
  ps.debugInfo = true;
#endif

  CompiledShader PS = shaderCompiler.Compile(ps);
  assert(PS.HasBlob() && "PS compile failed");

  // ==== GraphicsPipeline による一括構築 ====

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

  GraphicsPipeline pipeline;
  pipeline.Init(device);
  pipeline.Build(inputElems, _countof(inputElems), VS.Blob(), PS.Blob(),
                 cd.rtvFormat, cd.dsvFormat);

  // ==============================
  // カメラの初期化
  // ==============================

  CameraController camera;
  camera.Initialize(&input, Vector3{0.0f, 0.0f, -5.0f},
                    Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                    float(appConfig.width) / appConfig.height, 0.1f, 100.0f);

  //===============================
  // Textureをよんで転送する
  //===============================
  TextureManager texMgr;
  texMgr.Init(device, &core.SRV());

  // sRGBに寄せたいので true でロード（法線等のデータテクスチャは false 推奨）
  int tex_uvChecker = texMgr.LoadID("Resources/uvChecker.png", true);
  int tex_monsterBall = texMgr.LoadID("Resources/monsterBall.png", true);
  int tex_checker = texMgr.LoadID("Resources/checkerBoard.png", true);
  int tex_white = texMgr.LoadID("Resources/white1x1.png", true);

  //===========================
  // Sphere用
  //===========================

  Sphere *sphere = new Sphere();
  sphere->Initialize(device, 0.5f, 16, 16);

  bool isSphere = true;
  // 0:white1x1 / 1:uvChecker / 2:モンボ / 3:checkerBoard
  int sphereTextureIndex = 0;

  //============================
  // Viewportを設定する
  //============================
  D3D12_VIEWPORT viewport{};
  // クライアント領域のサイズと一緒にして画面全体に表示
  viewport.Width = static_cast<float>(appConfig.width);
  viewport.Height = static_cast<float>(appConfig.height);
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  //============================
  // Scissorを設定する
  //============================

  D3D12_RECT scissorRect{};
  // 基本的にビューポートと同じ矩形が構成されるようにする
  scissorRect.left = 0;
  scissorRect.top = 0;
  scissorRect.right = appConfig.width;
  scissorRect.bottom = appConfig.height;

  // ウィンドウのxボタンが押されるまでループ
  while (msg.message != WM_QUIT) {
    // メッセージがある場合は、メッセージを取得
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      // メッセージを処理
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {

      //======================================
      // ImGUiの処理
      //======================================

      imgui.NewFrame();

      ImGui::Begin("各種設定"); // ImGuiのウィンドウを開始

      if (ImGui::BeginTabBar("各種設定")) {

        if (ImGui::BeginTabItem("球")) {

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          ImGui::Checkbox("球の表示", &isSphere);

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          if (isSphere) {

            const char *sphereTextureItems[] = {
                "white1x1", "uvChecker", "モンスターボール", "checkerBoard"};
            ImGui::Combo("テクスチャ選択", &sphereTextureIndex,
                         sphereTextureItems, IM_ARRAYSIZE(sphereTextureItems));

            ImGui::Dummy(ImVec2(0.0f, 5.0f));
          }

          ImGui::EndTabItem();
        }

        ImGui::EndTabBar(); // 各種設定タブを終了
      }

      ImGui::End(); // ImGuiのウィンドウを終了

      camera.DrawImGui();

      input.Update(); // キー入力の更新

      camera.Update();

      auto mats = camera.GetMatrices();

      sphere->Update(mats.view, mats.proj);

      core.BeginFrame();
      UINT backBufferIndex = core.BackBufferIndex();
      ID3D12GraphicsCommandList *commandList = core.CL();

      ID3D12DescriptorHeap *heaps[] = {core.SRV().Heap()};
      commandList->SetDescriptorHeaps(1, heaps);

      core.Clear(0.1f, 0.25f, 0.5f, 1.0f);

      //============================
      // コマンドを積む 02_00
      //============================

      // ---- ビューポート／シザーの設定（描画領域の指定） ----
      commandList->RSSetViewports(1, &viewport);
      commandList->RSSetScissorRects(1, &scissorRect);

      // ---- ルートシグネチャ／PSO／頂点バッファ／トポロジの設定 ----
      // ルートシグネチャはPSOとは別にバインドが必要
      commandList->SetGraphicsRootSignature(pipeline.Root());
      commandList->SetPipelineState(pipeline.PSO()); // PSOを設定する
      // トポロジを設定する。PSOに設定しているものとはまた別。同じものを設定すると考えておけばいい
      commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      if (isSphere) {

        if (sphereTextureIndex == 0)
          sphere->SetTexture(texMgr.GetSrv(tex_white));
        else if (sphereTextureIndex == 1)
          sphere->SetTexture(texMgr.GetSrv(tex_uvChecker));
        else if (sphereTextureIndex == 2)
          sphere->SetTexture(texMgr.GetSrv(tex_monsterBall));
        else if (sphereTextureIndex == 3)
          sphere->SetTexture(texMgr.GetSrv(tex_checker));

        sphere->Draw(commandList);
      }

      imgui.Render(commandList);

      core.EndFrame();
    }
  }

  //===========================
  // 解放処理
  //===========================

  imgui.Shutdown();
  pipeline.Term();
  texMgr.Term();
  core.Term();

  delete sphere;

  // COMの終了処理
  CoUninitialize();

  return 0;
}
