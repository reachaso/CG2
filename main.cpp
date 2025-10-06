#pragma warning(push)
#pragma warning(disable : 28251)
#include "DebugCamera/DebugCamera.h"
#include "DirectXTex/DirectXTex.h"
#include "Dx12Core.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Input/Input.h"
#include "Log/Log.h"
#include "MainCamera/MainCamera.h"
#include "Math/Math.h"
#include "Model3D/Model3D.h"
#include "Sound/Sound.h"
#include "Sphere/Sphere.h"
#include "Texture/Texture2D/Texture2D.h"
#include "Texture/TextureManager/TextureManager.h"
#include "Window/Window.h"
#include "function/function.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "struct.h"
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <filesystem> //ファイルやディレクトリの操作を行うライブラリ
#include <format>     //文字列のフォーマットを行うライブラリ

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma warning(pop)

Log logger;

const char kWindowTitle[] = "GE";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

  // COMライブラリの初期化に失敗したらエラー
  HRESULT hrCoInt = CoInitializeEx(0, COINIT_MULTITHREADED);

  Window window; // ウィンドウの初期化
  window.Initialize(kWindowTitle, kClientWidth, kClientHeight);

  // DX12の初期化
  Dx12Core core;
  Dx12Core::Desc cd{};
  cd.width = kClientWidth;
  cd.height = kClientHeight;
  core.Init(window.hwnd, cd);

  Input input = Input(window.hwnd); // 入力の初期化

  MSG msg{};

  HRESULT hr = S_OK;

  // よく使うハンドル
  ID3D12Device *device = core.GetDevice();

  // dxcCompilerの初期化
  IDxcUtils *dxcUtils = nullptr;
  IDxcCompiler3 *dxcCompiler = nullptr;

  hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
  assert(SUCCEEDED(hr));
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
  assert(SUCCEEDED(hr));

  // includeに対応するための設定
  IDxcIncludeHandler *includeHandler = nullptr;
  hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
  assert(SUCCEEDED(hr));

  //==========================
  // Shaderをコンパイルする
  //==========================

  IDxcBlob *vertexShaderBlob =
      CompileShader(L"Shader/Object3D.VS.hlsl", L"vs_6_0", dxcUtils,
                    dxcCompiler, includeHandler);
  assert(vertexShaderBlob != nullptr);

  IDxcBlob *pixelShaderBlob =
      CompileShader(L"Shader/Object3D.PS.hlsl", L"ps_6_0", dxcUtils,
                    dxcCompiler, includeHandler);
  assert(pixelShaderBlob != nullptr);

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
  pipeline.Build(inputElems, _countof(inputElems), vertexShaderBlob,
                 pixelShaderBlob, cd.rtvFormat, cd.dsvFormat);

  //===========================
  // 平行光源用のデータを作成
  //===========================

  DirectionalLight directionalLight;

  directionalLight.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白色
  directionalLight.direction = Vector3(0.0f, -1.0f, 0.0f);  // 下向き
  directionalLight.intensity = 1.0f;                        // 強度1.0

  float len =
      std::sqrt(directionalLight.direction.x * directionalLight.direction.x +
                directionalLight.direction.y * directionalLight.direction.y +
                directionalLight.direction.z * directionalLight.direction.z);
  if (len > 0.0f) {
    directionalLight.direction.x /= len;
    directionalLight.direction.y /= len;
    directionalLight.direction.z /= len;
  }

  Sound Alarm01 = Sound();
  Alarm01.Initialize("Resources/Sounds/Alarm01.wav"); // サウンドの初期化

  DebugCamera debugCamera;
  debugCamera.Initialize(&input,
                         0.45f, // FOV
                         float(kClientWidth) / kClientHeight, 0.1f,
                         100.0f); // near, far

  MainCamera mainCamera;

  bool useDebugCamera = false; // デバッグカメラを使うかどうかのフラグ
  bool isGuide = false;        // ガイドを表示するかどうかのフラグ

  mainCamera.Initialize({0.0f, 0.0f, -5.0f}, // pos
                        {0.0f, 0.0f, 0.0f},  // rot
                        0.45f,               // fovY
                        float(kClientWidth) / kClientHeight, 0.1f,
                        100.0f // nearZ, farZ
  );

  //===========================
  // Sphere用
  //===========================

  Sphere *sphere = new Sphere();

  // 1フレームだけ使って初期化して問題なければOK
  core.BeginFrame();
  {
    ID3D12GraphicsCommandList *setupList = core.CL();
    if (sphere) {
      sphere->Initialize(device, setupList, 0.5f, 16, 16);
    }
  }
  core.EndFrame();

  bool eanableSphereRotateY = false;

  bool isSphere = false;

  // --- Sphere用のCBVリソースを作成 ---
  ID3D12Resource *sphereWvpResource =
      CreateBufferResource(device, sizeof(TransformationMatrix));
  TransformationMatrix *sphereWvpData = nullptr;
  sphereWvpResource->Map(0, nullptr, reinterpret_cast<void **>(&sphereWvpData));
  sphereWvpData->WVP = MakeIdentity4x4();   // 単位行列で初期化
  sphereWvpData->World = MakeIdentity4x4(); // 単位行列で初期化

  // --- Sphere用のMaterialリソースを作成 ---
  ID3D12Resource *sphereMaterialResource =
      CreateBufferResource(device, sizeof(Material));
  Material *sphereMaterialData = nullptr;
  sphereMaterialResource->Map(0, nullptr,
                              reinterpret_cast<void **>(&sphereMaterialData));
  // Materialの初期化
  sphereMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白色
  sphereMaterialData->uvTransform =
      MakeIdentity4x4(); // UV変換行列を単位行列で初期化
  sphereMaterialData->lightingMode = 2;

  // --- DirectionalLight用のCBVリソースを作成 ---
  ID3D12Resource *directionalLightResource =
      CreateBufferResource(device, sizeof(DirectionalLight));
  DirectionalLight *directionalLightData = nullptr;
  directionalLightResource->Map(
      0, nullptr, reinterpret_cast<void **>(&directionalLightData));
  *directionalLightData = directionalLight; // 初期値をコピー

  //===========================
  // sprite用の頂点リソースを作る
  //===========================

  ID3D12Resource *vertexResourceSprite =
      CreateBufferResource(device, sizeof(VertexData) * 4);

  // 頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
  // リソースの先頭のアドレスから使う
  vertexBufferViewSprite.BufferLocation =
      vertexResourceSprite->GetGPUVirtualAddress();
  // 使用するリソースのサイズは頂点4つ分のサイズ
  vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
  // 1つの頂点のサイズ
  vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

  // 頂点データを設定する
  VertexData *vertexDataSprite = nullptr;
  vertexResourceSprite->Map(0, nullptr,
                            reinterpret_cast<void **>(&vertexDataSprite));

  // 4点の情報
  vertexDataSprite[0].position = Vector4(0.0f, 360.0f, 0.0f, 1.0f); // 左下
  vertexDataSprite[0].texcoord = Vector2(0.0f, 1.0f); // 左下のテクスチャ座標
  vertexDataSprite[0].normal = Vector3(0.0f, 0.0f, -1.0f);
  vertexDataSprite[1].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f); // 左上
  vertexDataSprite[1].texcoord = Vector2(0.0f, 0.0f); // 左上のテクスチャ座標
  vertexDataSprite[1].normal = Vector3(0.0f, 0.0f, -1.0f);
  vertexDataSprite[2].position = Vector4(640.0f, 360.0f, 0.0f, 1.0f); // 右下
  vertexDataSprite[2].texcoord = Vector2(1.0f, 1.0f); // 右下のテクスチャ座標
  vertexDataSprite[2].normal = Vector3(0.0f, 0.0f, -1.0f);
  vertexDataSprite[3].position = Vector4(640.0f, 0.0f, 0.0f, 1.0f); // 右上
  vertexDataSprite[3].texcoord = Vector2(1.0f, 0.0f); // 右上のテクスチャ座標
  vertexDataSprite[3].normal = Vector3(0.0f, 0.0f, -1.0f);

  // Transform
  ID3D12Resource *transformationMatrixResourceSprite =
      CreateBufferResource(device, sizeof(TransformationMatrix));
  // データを書き込む
  TransformationMatrix *transformationMatrixDataSprite = nullptr;
  // 書き込むためのアドレスを取得
  transformationMatrixResourceSprite->Map(
      0, nullptr, reinterpret_cast<void **>(&transformationMatrixDataSprite));
  // 単位行列を設定
  transformationMatrixDataSprite->WVP = MakeIdentity4x4();
  transformationMatrixDataSprite->World = MakeIdentity4x4();

  Transform transformSprite{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  // Sprite用のWorldMatrixを作る
  Matrix4x4 worldMatrixSprite =
      MakeAffineMatrix(transformSprite.scale, transformSprite.rotation,
                       transformSprite.translation);
  Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
  Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
      0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
  Matrix4x4 WVPMatrixSprite = Multiply(
      worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
  transformationMatrixDataSprite->WVP = WVPMatrixSprite; // データを設定

  // sprite用のMaterialを作成する
  ID3D12Resource *spriteMaterialResource =
      CreateBufferResource(device, sizeof(Material));
  Material *spriteMaterialData = nullptr;
  spriteMaterialResource->Map(0, nullptr,
                              reinterpret_cast<void **>(&spriteMaterialData));
  // Materialの初期化
  spriteMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白色
  spriteMaterialData->uvTransform = MakeIdentity4x4();

  // インデックス用バッファリソースを作成（6頂点分の uint32_t インデックス）
  ID3D12Resource *indexResourceSprite =
      CreateBufferResource(device, sizeof(uint32_t) * 6);

  // インデックスバッファビューを作成
  D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
  indexBufferViewSprite.BufferLocation =
      indexResourceSprite->GetGPUVirtualAddress(); // バッファの先頭アドレス
  indexBufferViewSprite.SizeInBytes =
      sizeof(uint32_t) * 6; // インデックス数×サイズ
  indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

  // インデックスリソースにデータを転送する
  uint32_t *indexDataSprite = nullptr;
  indexResourceSprite->Map(0, nullptr,
                           reinterpret_cast<void **>(&indexDataSprite));

  indexDataSprite[0] = 0; // 左下
  indexDataSprite[1] = 1; // 左上
  indexDataSprite[2] = 2; // 右下
  indexDataSprite[3] = 1; // 左上
  indexDataSprite[4] = 3; // 右上
  indexDataSprite[5] = 2; // 右上

  //===========================
  // uv用
  //===========================

  Transform uvTransformSprite{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  bool isSprite = false;

  //==========================
  // VertexResourceを生成する
  //==========================

  ID3D12Resource *vertexResource =
      CreateBufferResource(device, sizeof(VertexData) * 6);

  //===========================
  // VertexBufferViewを生成する
  //===========================
  // 頂点バッファビューを生成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
  // リソースの先頭のアドレスから使う
  vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
  // 使用するリソースのサイズは頂点3つ分のサイズ
  vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;
  // 1つの頂点のサイズ
  vertexBufferView.StrideInBytes = sizeof(VertexData);

  //===========================
  // Resourceにデータを転送する
  //===========================

  VertexData *vertexData = nullptr;

  vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

  // １枚目
  //  左下
  vertexData[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
  vertexData[0].texcoord = Vector2(0.0f, 1.0f);

  // 上
  vertexData[1].position = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
  vertexData[1].texcoord = Vector2(0.5f, 0.0f);

  // 右下
  vertexData[2].position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
  vertexData[2].texcoord = Vector2(1.0f, 1.0f);

  // ２枚目
  //   左下
  vertexData[3].position = Vector4(-0.5f, -0.5f, 0.5f, 1.0f);
  vertexData[3].texcoord = Vector2(0.0f, 1.0f);

  // 上
  vertexData[4].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
  vertexData[4].texcoord = Vector2(0.5f, 0.0f);

  // 右下
  vertexData[5].position = Vector4(0.5f, -0.5f, -0.5f, 1.0f);
  vertexData[5].texcoord = Vector2(1.0f, 1.0f);

  Vector3 triNormal = {0.0f, 0.0f, -1.0f}; // 順序に合わせて符号を調整
  for (int i = 0; i < 6; ++i) {
    vertexData[i].normal = triNormal;
  }

  //============================
  // Viewportを設定する
  //============================
  D3D12_VIEWPORT viewport{};
  // クライアント領域のサイズと一緒にして画面全体に表示
  viewport.Width = kClientWidth;
  viewport.Height = kClientHeight;
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
  scissorRect.right = kClientWidth;
  scissorRect.bottom = kClientHeight;

  //===============================
  // Material用のResourceを生成する（三角形用）
  //===============================
  ID3D12Resource *triangleMaterialResource =
      CreateBufferResource(device, sizeof(Material));
  Material *triangleMaterialData = nullptr;
  triangleMaterialResource->Map(
      0, nullptr, reinterpret_cast<void **>(&triangleMaterialData));
  // Materialの初期化
  triangleMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白色
  triangleMaterialData->uvTransform = MakeIdentity4x4();
  triangleMaterialData->lightingMode = 0;

  // ============================================
  // 三角形のDirectionalLight用のCBVリソースを作成
  // ============================================

  ID3D12Resource *triangleDirectionalLightResource =
      CreateBufferResource(device, sizeof(DirectionalLight));
  DirectionalLight *triangleDirectionalLightData = nullptr;
  triangleDirectionalLightResource->Map(
      0, nullptr, reinterpret_cast<void **>(&triangleDirectionalLightData));
  *triangleDirectionalLightData = directionalLight; // 初期値をコピー

  //=================================================
  // TransformationMatrix用のResourceを生成する 02_02
  //=================================================

  ID3D12Resource *wvpResource =
      CreateBufferResource(device, sizeof(TransformationMatrix));
  // データを書き込む
  TransformationMatrix *wvpData = nullptr;
  // 書き込むためのアドレスを取得
  wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));
  // 単位行列を書き込んでおく
  wvpData->WVP = MakeIdentity4x4();
  wvpData->World = MakeIdentity4x4();

  //===============================
  // Transform変数を作る
  //===============================

  Transform transform{
      {1.0f, 1.0f, 1.0f}, // スケール（scale）
      {0.0f, 0.0f, 0.0f}, // 回転（rotation）
      {0.0f, 0.0f, 0.0f}, // 位置（translation）
  };

  //=====================================================
  // 三角形用
  //=====================================================
  // 色　デフォは白
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  // 回転のflag
  bool enableRotateX = false;
  bool enableRotateY = false;
  bool enableRotateZ = false;

  bool isTriangle1 = false;
  bool isTriangle2 = false;

  //===============================
  // ModelData
  //===============================

  bool isModelPlane = false;
  bool enableLighting = true;

  int modelIndex = 0;

  // ================
  // Model3D 準備
  // ================
  Model3D modelPlane, modelAxis, modelTeapot, modelBunny, modelSuzanne;

  modelPlane.Initialize(device);
  modelPlane.LoadObjGeometryLikeFunction("Resources", "plane.obj");

  modelAxis.Initialize(device);
  modelAxis.LoadObjGeometryLikeFunction("Resources", "axis.obj");

  modelTeapot.Initialize(device);
  modelTeapot.LoadObjGeometryLikeFunction("Resources", "teapot.obj");
  modelTeapot.SetLightingMode(None);

  modelBunny.Initialize(device);
  modelBunny.LoadObjGeometryLikeFunction("Resources", "bunny.obj");

  modelSuzanne.Initialize(device);
  modelSuzanne.LoadObjGeometryLikeFunction("Resources", "suzanne.obj");
  // suzanne の球面UV付与（旧mainの処理をクラスに移したメソッド）
  modelSuzanne.EnsureSphericalUVIfMissing();

  //===============================
  // Imguiの初期化
  //===============================

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  // ImGuiのフォント設定
  ImGuiIO &io = ImGui::GetIO();
  ImFont *fontJP = io.Fonts->AddFontFromFileTTF(
      "Resources/fonts/Huninn/Huninn-Regular.ttf", 15.0f, nullptr,
      io.Fonts->GetGlyphRangesJapanese());
  io.FontDefault = fontJP;

  // プラットフォーム／レンダラーの初期化
  ImGui_ImplWin32_Init(window.hwnd);
  ImGui_ImplDX12_Init(core.GetDevice(), core.FrameCount(),
                      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, core.SRV().Heap(),
                      core.SRV().CPUAt(0), core.SRV().GPUAt(0));

  core.SRV().AllocateCPU(1); // 先頭スロットをImGuiに確保

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

  // 0: uvChecker, 1: モンスターボール , 2: checkerBoard, 3: white1x1
  int triangleTextureIndex = 0;
  int sphereTextureIndex = 0;
  int modelTextureIndex = 0;

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

      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();

      ImGui::Begin("各種設定"); // ImGuiのウィンドウを開始

      if (ImGui::BeginTabBar("各種設定")) {
        if (ImGui::BeginTabItem("三角形")) {

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          ImGui::Checkbox("1枚目の三角形を表示", &isTriangle1);
          ImGui::SameLine();
          ImGui::Checkbox("2枚目の三角形を表示", &isTriangle2);
          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          if (isTriangle1 || isTriangle2) {

            const char *triangleTextureItems[] = {
                "white1x1", "uvChecker", "モンスターボール", "checkerBoard"};
            ImGui::Combo("テクスチャ選択", &triangleTextureIndex,
                         triangleTextureItems,
                         IM_ARRAYSIZE(triangleTextureItems));

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            // 三角形の描画設定
            if (ImGui::BeginTabBar("設定")) {
              if (ImGui::BeginTabItem("三角形設定")) {

                if (ImGui::CollapsingHeader("色",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::ColorEdit3("", &triangleMaterialData->color.x);
                  if (ImGui::Button("色をリセット")) {
                    triangleMaterialData->color =
                        Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("移動",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Translation", &transform.translation.x,
                                      -2.0f, 2.0f);
                  if (ImGui::Button("元の場所に戻す")) {
                    transform.translation = {0.0f, 0.0f, 0.0f};
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("回転",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Rotation", &transform.rotation.x,
                                      -2.0f, 2.0f);
                  if (ImGui::Button("回転行列のリセット")) {
                    transform.rotation = {0.0f, 0.0f, 0.0f};
                    enableRotateX = false;
                    enableRotateY = false;
                    enableRotateZ = false;
                  }
                  ImGui::SameLine();
                  ImGui::Dummy(ImVec2(20.0f, 0.0f)); // 20ピクセル分の空白
                  ImGui::SameLine();
                  ImGui::Checkbox("X軸回転", &enableRotateX);
                  ImGui::SameLine();
                  ImGui::Checkbox("Y軸回転", &enableRotateY);
                  ImGui::SameLine();
                  ImGui::Checkbox("Z軸回転", &enableRotateZ);
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("大きさ",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Scale", &transform.scale.x, 0.0f,
                                      4.0f);
                  if (ImGui::Button("大きさをリセット")) {
                    transform.scale = {1.0f, 1.0f, 1.0f};
                  }
                }
                ImGui::EndTabItem();
              }

              if (ImGui::BeginTabItem("ライト設定")) {
                if (ImGui::CollapsingHeader("ライティングモード",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::Combo("##Lighting Mode",
                               &triangleMaterialData->lightingMode,
                               "無し\0ランバート反射\0ハーフランバート\0");
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (triangleMaterialData->lightingMode != 0) {

                  if (ImGui::CollapsingHeader("ライトの色",
                                              ImGuiTreeNodeFlags_DefaultOpen)) {

                    if (ImGui::ColorEdit3(
                            "##ライトの色",
                            &triangleDirectionalLightData->color.x)) {
                      // ライトの色を更新
                      triangleDirectionalLightData->color =
                          triangleDirectionalLightData->color;
                    }

                    if (ImGui::Button("ライトの色のリセット")) {
                      triangleDirectionalLightData->color = {1.0f, 1.0f, 1.0f};
                    }
                  }

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));

                  if (ImGui::CollapsingHeader("ライトの方向",
                                              ImGuiTreeNodeFlags_DefaultOpen)) {

                    ImGui::SliderFloat3(
                        "##ライトの方向",
                        &triangleDirectionalLightData->direction.x, -1.0f,
                        1.0f);

                    if (ImGui::Button("ライトの方向をリセット")) {
                      triangleDirectionalLightData->direction = {0.0f, -1.0f,
                                                                 0.0f};
                    }

                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                  }

                  if (ImGui::CollapsingHeader("ライトの強さ",
                                              ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat("##Intensity",
                                     &triangleDirectionalLightData->intensity,
                                     0.01f, 0.0f, 1.0f);
                  }
                }
                ImGui::EndTabItem();
              }

              ImGui::EndTabBar();
            }
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("スプライト")) {

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          ImGui::Checkbox("スプライトの表示", &isSprite);

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          if (isSprite) {

            // スプライトの描画設定
            if (ImGui::BeginTabBar("設定")) {
              if (ImGui::BeginTabItem("スプライトの設定")) {

                if (ImGui::CollapsingHeader("位置",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat2("##Translation",
                                      &transformSprite.translation.x, 0.0f,
                                      1080.0f);
                  if (ImGui::Button("位置をリセット")) {
                    transformSprite.translation = {0.0f, 0.0f, 0.0f};
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("回転",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Rotation", &transformSprite.rotation.x,
                                      0.0f, 10.0f);
                  if (ImGui::Button("回転をリセット")) {
                    transformSprite.rotation = {0.0f, 0.0f, 0.0f};
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("大きさ",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat2("##Scale", &transformSprite.scale.x, 0.0f,
                                      10.0f);

                  if (ImGui::Button("大きさをリセット")) {
                    transformSprite.scale = {1.0f, 1.0f, 1.0f};
                  }
                }
                ImGui::EndTabItem();
              }

              if (ImGui::BeginTabItem("テクスチャの設定")) {

                if (ImGui::CollapsingHeader("テクスチャの表示位置",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {

                  ImGui::DragFloat2("##テクスチャの表示位置",
                                    &uvTransformSprite.translation.x, 0.01f,
                                    -10.0f, 10.0f);

                  if (ImGui::Button("テクスチャの位置をリセット")) {
                    uvTransformSprite.translation = Vector3(0.0f, 0.0f, 0.0f);
                  }

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));
                }

                if (ImGui::CollapsingHeader("テクスチャの大きさ",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::DragFloat2("##テクスチャの大きさ",
                                    &uvTransformSprite.scale.x, 0.01f, -10.0f,
                                    10.0f);

                  if (ImGui::Button("テクスチャの大きさをリセット")) {
                    uvTransformSprite.scale = Vector3(1.0f, 1.0f, 1.0f);
                  }

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));
                }

                if (ImGui::CollapsingHeader("テクスチャの回転",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderAngle("##テクスチャの回転",
                                     &uvTransformSprite.rotation.z);

                  if (ImGui::Button("テクスチャの回転をリセット")) {
                    uvTransformSprite.rotation = Vector3(0.0f, 0.0f, 0.0f);
                  }

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));
                }

                ImGui::EndTabItem();
              }
              ImGui::EndTabBar();
            }
          }
          ImGui::EndTabItem();
        }

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

            // 球体の描画設定
            if (ImGui::BeginTabBar("設定")) {
              if (ImGui::BeginTabItem("球の設定")) {
                if (ImGui::CollapsingHeader("位置",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {

                  ImGui::SliderFloat3("##Translation",
                                      &sphere->sphereTransform.translation.x,
                                      -3.0f, 3.0f);
                  if (ImGui::Button("位置のリセット")) {
                    sphere->sphereTransform.translation = {0.0f, 0.0f, 0.0f};
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("回転",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Rotation",
                                      &sphere->sphereTransform.rotation.x, 0.0f,
                                      10.0f);
                  if (ImGui::Button("回転のリセット")) {
                    sphere->sphereTransform.rotation = {0.0f, 0.0f, 0.0f};
                  }
                  ImGui::SameLine();
                  ImGui::Dummy(ImVec2(20.0f, 0.0f)); // 20ピクセル分の空白
                  ImGui::Checkbox("Y軸回転", &eanableSphereRotateY);
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("大きさ",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3(
                      "##Scale", &sphere->sphereTransform.scale.x, 0.0f, 10.0f);
                  if (ImGui::Button("大きさのリセット")) {
                    sphere->sphereTransform.scale = {1.0f, 1.0f, 1.0f};
                  }
                }
                ImGui::EndTabItem();
              }

              ImGui::Dummy(ImVec2(0.0f, 10.0f));

              if (ImGui::BeginTabItem("ライトの設定")) {

                if (ImGui::CollapsingHeader("ライティングモード",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::Combo("##Lighting Mode",
                               &sphereMaterialData->lightingMode,
                               "無し\0ランバート反射\0ハーフランバート\0");
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (sphereMaterialData->lightingMode != 0) {

                  if (ImGui::CollapsingHeader("ライトの色",
                                              ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::ColorEdit3("##Color", &directionalLight.color.x);
                    if (ImGui::Button("色のリセット")) {
                      directionalLight.color = {1.0f, 1.0f, 1.0f};
                    }
                  }

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));

                  if (ImGui::CollapsingHeader("ライトの向き",
                                              ImGuiTreeNodeFlags_DefaultOpen)) {
                    // 平行光源の方向を設定するスライダー
                    ImGui::SliderFloat3("##Direction",
                                        &directionalLight.direction.x, -1.0f,
                                        1.0f);
                    // 方向をリセットするボタン
                    if (ImGui::Button("向きのリセット")) {
                      directionalLight.direction = {0.0f, -1.0f, 0.0f};
                    }
                  }

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));

                  if (ImGui::CollapsingHeader("ライトの強さ",
                                              ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat("##Intensity", &directionalLight.intensity,
                                     0.01f, 0.0f, 1.0f);
                  }
                }

                ImGui::EndTabItem();
              }
              ImGui::EndTabBar();
            }
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("3Dモデル")) {

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          ImGui::Checkbox("3Dモデルの表示", &isModelPlane);

          ImGui::Dummy(ImVec2(0.0f, 5.0f));

          if (isModelPlane) {

            const char *modelItems[] = {"Plane", "axis", "ティーポット",
                                        "バニー", "スザンヌ"};
            ImGui::Combo("モデル選択", &modelIndex, modelItems,
                         IM_ARRAYSIZE(modelItems));

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            const char *modelTextureItems[] = {
                "white1x1", "uvChecker", "モンスターボール", "checkerBoard"};
            ImGui::Combo("テクスチャ選択", &modelTextureIndex,
                         modelTextureItems, IM_ARRAYSIZE(modelTextureItems));

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            if (ImGui::BeginTabBar("設定")) {

              if (ImGui::BeginTabItem("3Dモデル設定")) {
                ImGui::EndTabItem(); // ライト設定タブを終了
              }
              ImGui::EndTabBar(); // 3Dモデル - 設定タブを終了
            }
          }
          ImGui::EndTabItem(); // 3Dモデルタブを終了
        }

        if (ImGui::BeginTabItem("サウンド")) { // サウンドタブを開始

          Alarm01.SoundImGui("Alarm01");

          ImGui::EndTabItem(); // サウンドタブを終了
        }

        if (ImGui::BeginTabItem("コントローラー")) {

          input.ControllerImGui();

          ImGui::EndTabItem(); // コントローラータブを終了
        }

        ImGui::EndTabBar(); // 各種設定タブを終了
      }

      ImGui::End(); // ImGuiのウィンドウを終了

      ImGui::Begin("カメラモード : Tab");

      if (useDebugCamera) {
        ImGui::Text("デバッグカメラモード");
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        if (ImGui::Button("リセット")) {
          debugCamera.Reset(); // デバッグカメラの位置をリセット
        }

        ImGui::SameLine();

        if (ImGui::Button("操作方法")) {
          isGuide = !isGuide; // 操作方法の表示を切り替え
        }
      } else {
        ImGui::Text("メインカメラモード");
      }

      ImGui::End();

      if (isGuide) {
        ImGui::Begin("操作方法", &isGuide, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text(" WASD : 前後左右移動");
        ImGui::Text(" QE : 上下移動");
        ImGui::Text(" 十字キー : カメラの回転");
        ImGui::Text(" マウスホイール : 前後移動");
        ImGui::Text(" 左クリックしながらドラッグ : 回転");
        ImGui::Text(" マウスホイール押しながらドラッグ : 上下左右移動");

        ImGui::End();
      }

      input.Update(); // キー入力の更新

      if (input.IsKeyTrigger(DIK_TAB)) {
        useDebugCamera = !useDebugCamera; // Tabキーでカメラモードを切り替え
      }

      if (useDebugCamera) {
        debugCamera.Update(); // デバックカメラの入力
      } else {
        mainCamera.Update(); // メインカメラの入力
      }

      const Matrix4x4 &view =
          useDebugCamera ? debugCamera.GetView() : mainCamera.GetView();
      const Matrix4x4 &proj = useDebugCamera ? debugCamera.GetProjection()
                                             : mainCamera.GetProjection();

      core.BeginFrame();
      UINT backBufferIndex = core.BackBufferIndex();
      ID3D12GraphicsCommandList *commandList = core.CL();

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
      commandList->SetPipelineState(pipeline.PSO());            // PSOを設定する
      commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBWを設定
      // トポロジを設定する。PSOに設定しているものとはまた別。同じものを設定すると考えておけばいい
      commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      //===============================
      // マテリアル用のCBVを設定する 02_01
      //===============================
      commandList->SetGraphicsRootConstantBufferView(
          0, triangleMaterialResource->GetGPUVirtualAddress());

      //===========================================
      // TransformationMatrix用のCBVを設定する 02_02
      //===========================================
      commandList->SetGraphicsRootConstantBufferView(
          1, wvpResource->GetGPUVirtualAddress());

      if (isTriangle1) {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        // Material用CBVをセット
        commandList->SetGraphicsRootConstantBufferView(
            0, triangleMaterialResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(
            3, directionalLightResource->GetGPUVirtualAddress());

        if (triangleTextureIndex == 0) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_white)); // white1x1
        } else if (triangleTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_uvChecker)); // uvChecker
        } else if (triangleTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_monsterBall)); // モンスターボール
        } else if (triangleTextureIndex == 3) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_checker)); // checkerBoard
        }

        commandList->SetGraphicsRootConstantBufferView(
            3, triangleDirectionalLightResource->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 0, 0);
      }
      if (isTriangle2) {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        // Material用CBVをセット
        commandList->SetGraphicsRootConstantBufferView(
            0, triangleMaterialResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(
            3, directionalLightResource->GetGPUVirtualAddress());

        if (triangleTextureIndex == 0) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_white)); // white1x1
        } else if (triangleTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_uvChecker)); // uvChecker
        } else if (triangleTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_monsterBall)); // モンスターボール
        } else if (triangleTextureIndex == 3) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_checker)); // checkerBoard
        }

        commandList->SetGraphicsRootConstantBufferView(
            3, triangleDirectionalLightResource->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 3, 0);
      }

      if (isSphere) {
        // Material用CBV
        commandList->SetGraphicsRootConstantBufferView(
            0, sphereMaterialResource->GetGPUVirtualAddress());
        // WVP用CBV
        commandList->SetGraphicsRootConstantBufferView(
            1, sphereWvpResource->GetGPUVirtualAddress());

        if (sphereTextureIndex == 0) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_white)); // white1x1
        } else if (sphereTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_uvChecker)); // uvChecker
        } else if (sphereTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_monsterBall)); // モンスターボール
        } else if (sphereTextureIndex == 3) {
          commandList->SetGraphicsRootDescriptorTable(
              2, texMgr.GetSrv(tex_checker)); // checkerBoard
        }

        // DirectionalLight用CBV
        commandList->SetGraphicsRootConstantBufferView(
            3, directionalLightResource->GetGPUVirtualAddress());

        sphere->Draw(commandList);
      }

      // 置き換え後
      if (isModelPlane) {
        // 1) 今選んでるモデルを取る
        Model3D *selected = (modelIndex == 0)   ? &modelPlane
                            : (modelIndex == 1) ? &modelAxis
                            : (modelIndex == 2) ? &modelTeapot
                            : (modelIndex == 3) ? &modelBunny
                                                : &modelSuzanne;

        // 3) 使うテクスチャ（SRV）を main 側で選んで渡す
        D3D12_GPU_DESCRIPTOR_HANDLE srv =
            (modelTextureIndex == 0) ? texMgr.GetSrv(tex_white) : // white1x1
                (modelTextureIndex == 1) ? texMgr.GetSrv(tex_uvChecker)
                                         : // uvChecker
                (modelTextureIndex == 2) ? texMgr.GetSrv(tex_monsterBall)
                                         :  // モンボ
                texMgr.GetSrv(tex_checker); // checkerBoard

        selected->SetTexture(srv);

        // 4) 行列更新 → 描画
        //    RootParam(0:Material,1:WVP,2:SRV,3:Light) のセットや VB セットは
        //    Draw() が内部で行う
        selected->Update(view, proj);
        selected->Draw(commandList);
      }

      if (isSprite) {

        // spriteのMaterial用CBVを設定
        commandList->SetGraphicsRootConstantBufferView(
            0, spriteMaterialResource->GetGPUVirtualAddress());
        // spriteのWVP用CBVを設定
        commandList->SetGraphicsRootConstantBufferView(
            1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
        // spriteのSRVを設定
        commandList->SetGraphicsRootDescriptorTable(
            2, texMgr.GetSrv(tex_uvChecker)); // テクスチャのSRVを設定

        // --- スプライト描画 ---
        commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
        commandList->IASetIndexBuffer(&indexBufferViewSprite);
        commandList->SetGraphicsRootConstantBufferView(
            1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
        commandList->DrawIndexedInstanced(6, 1, 0, 0,
                                          0); // 6頂点、開始インデックス0
      }

      ImGui::Render();

      ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

      core.EndFrame();

      //============================
      // Transformを更新する
      //============================

      float deltaTime = 1.0f / 60.0f; // フレームレートを60FPSと仮定

      if (enableRotateX) {
        transform.rotation.x += 0.03f * deltaTime;
      }
      if (enableRotateY) {
        transform.rotation.y += 0.03f * deltaTime;
      }
      if (enableRotateZ) {
        transform.rotation.z += 0.03f * deltaTime;
      }

      Matrix4x4 worldMatrix = MakeAffineMatrix(
          transform.scale, transform.rotation, transform.translation);

      Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(view, proj));

      wvpData->WVP = wvpMatrix;
      wvpData->World = worldMatrix;

      //======================================
      // スプライト用の行列・UV変換の更新
      //======================================

      // --- Sprite用のWVP行列を更新 ---
      Matrix4x4 worldMatrixSprite =
          MakeAffineMatrix(transformSprite.scale, transformSprite.rotation,
                           transformSprite.translation);
      Matrix4x4 WVPMatrixSprite =
          Multiply(worldMatrixSprite,
                   Multiply(viewMatrixSprite, projectionMatrixSprite));
      transformationMatrixDataSprite->WVP = WVPMatrixSprite;

      Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
      uvTransformMatrix = Multiply(
          uvTransformMatrix, MakeRotateMatrix(Z, uvTransformSprite.rotation.z));
      uvTransformMatrix =
          Multiply(uvTransformMatrix,
                   MakeTranslateMatrix(uvTransformSprite.translation));
      spriteMaterialData->uvTransform = uvTransformMatrix;
    }
  }

  //===========================
  // 解放処理
  //===========================

  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  vertexResource->Release();
  vertexResourceSprite->Release();
  transformationMatrixResourceSprite->Release();
  sphereWvpResource->Release();

  pipeline.Term();
  pixelShaderBlob->Release();
  vertexShaderBlob->Release();
  wvpResource->Release();
  dxcUtils->Release();
  dxcCompiler->Release();
  includeHandler->Release();

  delete sphere;

  texMgr.Term();

  indexResourceSprite->Release();
  spriteMaterialResource->Release();

  triangleMaterialResource->Release();
  triangleDirectionalLightResource->Release();
  directionalLightResource->Release();

  core.Term();

  // COMの終了処理
  CoUninitialize();

  return 0;
}
