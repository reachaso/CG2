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
#include "PipelineManager.h"
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
  Window *window = nullptr;
  window = new Window();
  window->Initialize(appConfig.title.c_str(), appConfig.width,
                     appConfig.height);

  // ===========================
  // DX12の初期化
  // ===========================
  Dx12Core core;
  Dx12Core::Desc cd{};
  cd.width = appConfig.width;
  cd.height = appConfig.height;
  core.Init(window->GetHwnd(), cd);

  // よく使うハンドル
  ID3D12Device *device = core.GetDevice();

  // ===========================
  // Inputの初期化
  // ===========================
  Input input = Input(window->GetHwnd());

  //===============================
  // Imguiの初期化
  //===============================

  ImGuiManager imgui;
  imgui.Init(window->GetHwnd(), core);

  // ==========================
  // メッセージループ
  // ==========================
  MSG msg{};

  // ===========================
  // PipelineManager 初期化
  // ===========================
  PipelineManager pm;
  pm.Init(device, cd.rtvFormat, cd.dsvFormat);

  // ==== GraphicsPipeline による一括構築 ====
  GraphicsPipeline *pipeObj = pm.CreateFromFiles(
      "object3d", L"Shader/Object3D.VS.hlsl", L"Shader/Object3D.PS.hlsl",InputLayoutType::Object3D);

  // ==============================
  // カメラの初期化
  // ==============================

  CameraController camera;
  camera.Initialize(&input, Vector3{0.0f, 0.0f, -5.0f},
                    Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                    float(appConfig.width) / appConfig.height, 0.1f, 100.0f);

  //===============================
  // Texture
  //===============================
  TextureManager texMgr;
  texMgr.Init(device, &core.SRV());

  // sRGBに寄せたいので true でロード（法線等のデータテクスチャは false 推奨）
  int tex_uvChecker = texMgr.LoadID("Resources/uvChecker.png", true);
  int tex_monsterBall = texMgr.LoadID("Resources/monsterBall.png", true);
  int tex_checker = texMgr.LoadID("Resources/checkerBoard.png", true);
  int tex_white = texMgr.LoadID("Resources/white1x1.png", true);

  //===========================
  // Sphere
  //===========================

  Sphere *sphere = new Sphere();
  sphere->Initialize(device, 0.5f, 16, 16);

  bool isSphere = true;
  // 0:white1x1 / 1:uvChecker / 2:モンボ / 3:checkerBoard
  int sphereTextureIndex = 0;

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

      //======================================
      // 更新処理
      //======================================

      input.Update();

      camera.Update();

      CameraMatrices mats = camera.GetMatrices();

      sphere->Update(mats.view, mats.proj);

      // =====================================
      // 更新処理ここまで
      // =====================================

      //======================================
      // 描画処理
      //======================================

      ID3D12GraphicsCommandList *commandList = core.CL();
      core.BeginFrame();

      //============================
      // コマンドを積む 02_00
      //============================

      // ---- ルートシグネチャ／PSO／頂点バッファ／トポロジの設定 ----
      commandList->SetGraphicsRootSignature(pipeObj->Root());
      commandList->SetPipelineState(pipeObj->PSO());
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

      // ==========================
      // 描画処理ここまで
      // ==========================
    }
  }

  //===========================
  // 解放処理
  //===========================

  imgui.Shutdown();
  pm.Term();
  texMgr.Term();
  delete window;
  core.Term();

  delete sphere;

  // COMの終了処理
  CoUninitialize();

  return 0;
}
