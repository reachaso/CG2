#pragma warning(push)
#pragma warning(disable : 28251)
#include "Affine/Affine.h"
#include "DebugCamera/DebugCamera.h"
#include "Input/Input.h"
#include "Log/Log.h"
#include "MainCamera/MainCamera.h"
#include "Sound/Sound.h"
#include "Sphere/Sphere.h"
#include "Window/Window.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "function/function.h"
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

const char kWindowTitle[] = "CG2";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

  // COMライブラリの初期化
  CoInitializeEx(0, COINIT_MULTITHREADED);
  // COMライブラリの初期化に失敗したらエラー
  HRESULT hrCoInt = CoInitializeEx(0, COINIT_MULTITHREADED);

  Window window; // ウィンドウの初期化
  window.Initialize(kWindowTitle, kClientWidth, kClientHeight);

  Sound Alarm01 = Sound();
  Alarm01.Initialize("Resources/Sounds/Alarm01.wav"); // サウンドの初期化

  Sound Alarm02 = Sound();
  Alarm02.Initialize("Resources/Sounds/Alarm02.wav"); // サウンドの初期化

  Sound fanfare = Sound();
  fanfare.Initialize("Resources/Sounds/fanfare.wav"); // サウンドの初期化

  Input input = Input(window.hwnd); // 入力の初期化

  DebugCamera debugCamera;
  debugCamera.Initialize(&input,
                         0.45f, // FOV
                         float(kClientWidth) / kClientHeight, 0.1f,
                         100.0f); // near, far

  MainCamera mainCamera;

  bool useDebugCamera = false; // デバッグカメラを使うかどうかのフラグ

  mainCamera.Initialize({0.0f, 0.0f, -5.0f}, // pos
                        {0.0f, 0.0f, 0.0f},  // rot
                        0.45f,               // fovY
                        float(kClientWidth) / kClientHeight, 0.1f,
                        100.0f // nearZ, farZ
  );

#ifdef _DEBUG
  ID3D12Debug1 *debugContoroller = nullptr;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugContoroller)))) {
    // デバッグレイヤーを有効にする
    debugContoroller->EnableDebugLayer();
    // さらにGPU側でもチェックを行う
    debugContoroller->SetEnableGPUBasedValidation(true);
  }
#endif

  MSG msg{};

  // DXGIファクトリーの生成
  IDXGIFactory6 *dxgiFactory = nullptr;
  // HRESULTはWindows系のエラーコードであり、
  // 関数が成功したかどうかをSUCCEEDEDマクロで判定する
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
  // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合多いのでassertでエラーを出す
  assert(SUCCEEDED(hr));

  // 使用すうるアダプタ用の変数。最初にnullptrを入れておく
  IDXGIAdapter4 *useAdapter = nullptr;

  // 良い順にアダプタを頼む
  for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
                       i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                       IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
       ++i) {
    // アダプタの情報を取得するための変数
    DXGI_ADAPTER_DESC3 adapterDesc{};
    hr = useAdapter->GetDesc3(&adapterDesc);
    assert(SUCCEEDED(hr)); // 取得できないのは一大事
    // ソフトウェアアダプタは除外
    if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      // 採用したアダプタの情報をログに出力。wstringの方なので注意
      logger.WriteLog(logger.ConvertString(
          std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
      break;
    }
    useAdapter = nullptr; // ソフトウェアアダプタは除外するのでnullptrに戻す
  }
  // アダプタが見つからなかった場合はエラー
  assert(useAdapter != nullptr);

  ID3D12Device *device = nullptr;
  // 機能レベルとログの出力用の文字列
  D3D_FEATURE_LEVEL featureLevel[] = {
      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};

  const char *featureLevelStrings[] = {"12.2", "12.1", "12.0"};

  // 高い順に機能レベルを試す
  for (size_t i = 0; i < std::size(featureLevel); ++i) {
    // デバイスの生成
    hr = D3D12CreateDevice(useAdapter, featureLevel[i], IID_PPV_ARGS(&device));
    // 指定した機能レベルでデバイスが生成できた場合
    if (SUCCEEDED(hr)) {
      // 生成できたのでログ出力を行ってループを抜ける
      logger.WriteLog(
          std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
      break;
    }
  }
  // デバイスが生成できなかった場合はエラー
  assert(device != nullptr);
  logger.WriteLog("Complete create D3D12Device!!!\n"); // 初期化完了のログ出力

  // DescriptorSizeを取得しておく
  const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  const uint32_t descriptorSizeRTV =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  const uint32_t descriptorSizeDSV =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

#ifdef _DEBUG
  ID3D12InfoQueue *infoQueue = nullptr;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&infoQueue)))) {
    // やばいエラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    // エラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    // 警告時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
    // 解放
    infoQueue->Release();

    D3D12_MESSAGE_ID denyIds[] = {
        // Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
    };
    // 抑制するレベル
    D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
    D3D12_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    // 指定したメッセージの表示を抑制する
    infoQueue->AddStorageFilterEntries(&filter);
  }
#endif

  // コマンドキューの生成
  ID3D12CommandQueue *commandQueue = nullptr;
  D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
  hr = device->CreateCommandQueue(&commandQueueDesc,
                                  IID_PPV_ARGS(&commandQueue));
  // コマンドキューの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  // コマンドアロケータの生成
  ID3D12CommandAllocator *commandAllocator = nullptr;
  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&commandAllocator));
  // コマンドアロケータの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  // コマンドリストの生成
  ID3D12GraphicsCommandList *commandList = nullptr;
  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 commandAllocator, nullptr,
                                 IID_PPV_ARGS(&commandList));
  // コマンドリストの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  // スワップチェーンを生成する
  IDXGISwapChain4 *swapChain = nullptr;
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.Width = kClientWidth;                // スワップチェーンの幅
  swapChainDesc.Height = kClientHeight;              // スワップチェーンの高さ
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
  swapChainDesc.SampleDesc.Count = 1;                // マルチサンプルしない
  swapChainDesc.BufferUsage =
      DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして使用
  swapChainDesc.BufferCount = 2;       // バッファの数
  swapChainDesc.SwapEffect =
      DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターにうつしたら中身を廃棄

  // コマンドキュー、ウィンドウハンドル設定を潰して生成する
  hr = dxgiFactory->CreateSwapChainForHwnd(
      commandQueue, window.hwnd, &swapChainDesc, nullptr, nullptr,
      reinterpret_cast<IDXGISwapChain1 **>(&swapChain));
  // スワップチェーンの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  // ディスクリプタヒープの生成
  ID3D12DescriptorHeap *rtvDescriptorHeap =
      CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2,
                           false); // RenderTargetView用のヒープを生成する

  ID3D12DescriptorHeap *srvDescriptorHeap =
      CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128,
                           true); // ShaderResourceView用のヒープを生成する

  // SwapChainからResourceを引っ張ってくる
  ID3D12Resource *swapChainResource[2] = {nullptr};
  hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResource[0]));
  // 上手く取得できなかった場合はエラー
  assert(SUCCEEDED(hr));
  hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResource[1]));
  // 上手く取得できなかった場合はエラー
  assert(SUCCEEDED(hr));

  // RTVを生成する
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換する
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // テクスチャ2D
  // ディスクリプタの先頭を取得
  D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
      rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  // スワップチェーンの数だけRTVを生成する
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
  // まず一つ目を作る
  rtvHandles[0] = rtvStartHandle;
  device->CreateRenderTargetView(swapChainResource[0], &rtvDesc, rtvHandles[0]);
  // 二つ目を作る
  rtvHandles[1].ptr =
      rtvHandles[0].ptr +
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  device->CreateRenderTargetView(swapChainResource[1], &rtvDesc, rtvHandles[1]);

  // 初期値0でFenceを作る
  ID3D12Fence *fence = nullptr;
  uint64_t fenceValue = 0;
  hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
                           IID_PPV_ARGS(&fence));

  assert(SUCCEEDED(hr));

  // FenceのSignalを待つためのイベントを作る
  HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent != nullptr);

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

  //=====================================================
  // PSO 02-00
  //=====================================================

  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
  descriptionRootSignature.Flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // RootParameter作成。複数設定できるので配列。今回は結果1つだけなので長さ1のは配列
  // 02-01
  D3D12_ROOT_PARAMETER rootParameters[4] = {};
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
  rootParameters[0].ShaderVisibility =
      D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーからアクセスする
  rootParameters[0].Descriptor.ShaderRegister = 0; // シェーダーのレジスタ番号
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
  rootParameters[1].ShaderVisibility =
      D3D12_SHADER_VISIBILITY_VERTEX;              // Vertexからアクセスする
  rootParameters[1].Descriptor.ShaderRegister = 0; // シェーダーのレジスタ番号
  descriptionRootSignature.pParameters =
      rootParameters; // ルートパラメータ配列へのポインタ
  descriptionRootSignature.NumParameters =
      _countof(rootParameters); // ルートパラメータの数

  D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
  descriptorRange[0].BaseShaderRegister = 0; // シェーダーのレジスタ番号
  descriptorRange[0].NumDescriptors = 1;     // SRVの数
  descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRange[0].OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // デスクリプタテーブルの先頭からのオフセット

  rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParameters[2].ShaderVisibility =
      D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーからアクセスする
  rootParameters[2].DescriptorTable.pDescriptorRanges =
      descriptorRange; // Tableの中身の配列を指定
  rootParameters[2].DescriptorTable.NumDescriptorRanges =
      _countof(descriptorRange); // Tableの中身の数を指定

  rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[3].ShaderVisibility =
      D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーからアクセスする
  rootParameters[3].Descriptor.ShaderRegister = 1; // シェーダーのレジスタ番号

  //=========================
  // RootSignatureを生成する
  //=========================

  D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
  staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
  staticSamplers[0].ShaderRegister = 0;
  staticSamplers[0].ShaderVisibility =
      D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーからアクセスする

  descriptionRootSignature.pStaticSamplers =
      staticSamplers; // スタティックサンプラーの配列へのポインタ
  descriptionRootSignature.NumStaticSamplers =
      _countof(staticSamplers); // スタティックサンプラーの数

  // シリアライズしてバイナリにする
  ID3DBlob *signatureBlob = nullptr;
  ID3DBlob *errorBlob = nullptr;

  hr = D3D12SerializeRootSignature(&descriptionRootSignature,
                                   D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
                                   &errorBlob);

  if (FAILED(hr)) {
    logger.WriteLog(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
    assert(false);
  }

  // バイナリを元にRootSignatureを生成する
  ID3D12RootSignature *rootSignature = nullptr;
  hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                   signatureBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&rootSignature));

  // RootSignatureの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  //=========================
  // inputLayoutを設定する
  //=========================

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
  inputElementDescs[0].SemanticName = "POSITION"; // セマンティクス名
  inputElementDescs[0].SemanticIndex = 0;         // セマンティクスインデックス
  inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  inputElementDescs[1].SemanticName = "TEXCOORD"; // セマンティクス名
  inputElementDescs[1].SemanticIndex = 0;         // セマンティクスインデックス
  inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  inputElementDescs[2].SemanticName = "NORMAL"; // セマンティクス名
  inputElementDescs[2].SemanticIndex = 0;       // セマンティクスインデックス
  inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = inputElementDescs;
  inputLayoutDesc.NumElements = _countof(inputElementDescs);

  //=========================
  // BlendStateを設定する
  //=========================

  D3D12_BLEND_DESC blendDesc{};

  // 全ての色要素を書き込む
  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;

  //==========================
  // RasterizerStateを設定する
  //==========================
  D3D12_RASTERIZER_DESC rasterizerDesc{};
  // 裏面(時計回り)を表示しない
  rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
  // 三角形の中を塗りつぶす
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

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

  //==========================
  // PSOを生成する
  //==========================

  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = rootSignature;
  graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
  graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(),
                                  vertexShaderBlob->GetBufferSize()};
  graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(),
                                  pixelShaderBlob->GetBufferSize()};
  graphicsPipelineStateDesc.BlendState = blendDesc;
  graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
  // 書き込むRTVの情報
  graphicsPipelineStateDesc.NumRenderTargets = 1;
  graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  // 利用するトポロジ(形状)のタイプ。三角形
  graphicsPipelineStateDesc.PrimitiveTopologyType =
      D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // どのように画面に色を打ち込むかの設定
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
  // 深度テストを有効にする
  depthStencilDesc.DepthEnable = true;
  // 書き込み
  depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  // 深度比較関数を設定
  depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

  // DepthStencilの設定
  graphicsPipelineStateDesc.DepthStencilState =
      depthStencilDesc; // 深度ステンシルの設定を追加
  graphicsPipelineStateDesc.DSVFormat =
      DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度ステンシルのフォーマット

  // 実際に生成
  ID3D12PipelineState *graphicsPipelineState = nullptr;
  hr = device->CreateGraphicsPipelineState(
      &graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
  // PSOの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

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

  //===========================
  // Sphere用
  //===========================

  Sphere *sphere = new Sphere();

  if (sphere != nullptr) {
    sphere->Initialize(device, commandList, 0.5f, 16, 16);
  }

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
  // 使用するリソースのサイズは頂点6つ分のサイズ
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
  indexDataSprite[4] = 3; // 左上
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

  // 深度ステンシル用のResourceを生成する
  ID3D12Resource *depthStencilResource =
      CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

  ID3D12DescriptorHeap *dsvDescriptorHeap =
      CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

  // DSVの設定
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format =
      DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度24ビット、ステンシル8ビット
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // テクスチャ2D

  device->CreateDepthStencilView(
      depthStencilResource, &dsvDesc,
      dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

  // 描画先のRTVとDSVを設定する
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
      dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

  // バックバッファのインデックスを取得
  UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

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
      {1.0f, 1.0f, 1.0f}, // 位置
      {0.0f, 0.0f, 0.0f}, // 回転
      {0.0f, 0.0f, 0.0f}, // スケール
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

  //=========================
  // planeModel用の頂点リソースを作る
  //=========================

  ModelData planeModel = LoadObjFile("Resources", "plane.obj");

  // モデル用テクスチャを読み込む
  DirectX::ScratchImage mipImagesPlaneModel =
      LoadTexture(planeModel.material.textureFilePath);
  const DirectX::TexMetadata &metadataModel = mipImagesPlaneModel.GetMetadata();
  ID3D12Resource *textureResourceModel =
      CreateTextureResource(device, metadataModel);
  UploadTextureData(textureResourceModel, mipImagesPlaneModel);

  // 頂点リソースを作る
  ID3D12Resource *vertexResourcePlaneModel = CreateBufferResource(
      device, sizeof(VertexData) * planeModel.vertices.size());
  // 頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewPlaneModel{};
  vertexBufferViewPlaneModel.BufferLocation =
      vertexResourcePlaneModel
          ->GetGPUVirtualAddress(); // リソースの先頭のアドレスからバッファ
  vertexBufferViewPlaneModel.SizeInBytes =
      UINT(sizeof(VertexData) *
           planeModel.vertices.size()); // 頂点リソースは頂点のサイズ
  vertexBufferViewPlaneModel.StrideInBytes =
      sizeof(VertexData); // 1頂点あたりのサイズ

  // 頂点リソースにデータを書き込む
  VertexData *vertexDataPlaneModel = nullptr;
  vertexResourcePlaneModel->Map(
      0, nullptr,
      reinterpret_cast<void **>(
          &vertexDataPlaneModel)); // 書き込むためのアドレスを取得
  std::memcpy(vertexDataPlaneModel, planeModel.vertices.data(),
              sizeof(VertexData) *
                  planeModel.vertices.size()); // 頂点データをリソースにコピー

  //=============================
  // axisModel用の頂点リソースを作る
  //=============================

  ModelData axisModel = LoadObjFile("Resources", "axis.obj");

  DirectX::ScratchImage mipImagesAxisModel =
      LoadTexture(axisModel.material.textureFilePath);
  const DirectX::TexMetadata &metadataAxis = mipImagesAxisModel.GetMetadata();
  ID3D12Resource *textureResourceAxis =
      CreateTextureResource(device, metadataAxis);
  UploadTextureData(textureResourceAxis, mipImagesAxisModel);

  // 頂点リソースを作る
  ID3D12Resource *vertexResourceAxisModel = CreateBufferResource(
      device, sizeof(VertexData) * axisModel.vertices.size());
  // 頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewAxisModel{};
  vertexBufferViewAxisModel.BufferLocation =
      vertexResourceAxisModel
          ->GetGPUVirtualAddress(); // リソースの先頭のアドレスからバッファ
  vertexBufferViewAxisModel.SizeInBytes =
      UINT(sizeof(VertexData) *
           axisModel.vertices.size()); // 頂点リソースは頂点のサイズ
  vertexBufferViewAxisModel.StrideInBytes =
      sizeof(VertexData); // 1頂点あたりのサイズ

  // 頂点リソースにデータを書き込む
  VertexData *vertexDataAxisModel = nullptr;
  vertexResourceAxisModel->Map(
      0, nullptr,
      reinterpret_cast<void **>(
          &vertexDataAxisModel)); // 書き込むためのアドレスを取得
  std::memcpy(vertexDataAxisModel, axisModel.vertices.data(),
              sizeof(VertexData) *
                  axisModel.vertices.size()); // 頂点データをリソースにコピー

  //=============================
  // axisModel用の頂点リソースを作る
  //=============================

  ModelData teapotModel = LoadObjFile("Resources", "teapot.obj");

  DirectX::ScratchImage mipImagesTeapotModel =
      LoadTexture(teapotModel.material.textureFilePath);
  const DirectX::TexMetadata &metadataTeapot =
      mipImagesTeapotModel.GetMetadata();
  ID3D12Resource *textureResourceTeapot =
      CreateTextureResource(device, metadataTeapot);
  UploadTextureData(textureResourceTeapot, mipImagesTeapotModel);

  // 頂点リソースを作る
  ID3D12Resource *vertexResourceTeapotModel = CreateBufferResource(
      device, sizeof(VertexData) * teapotModel.vertices.size());
  // 頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewTeapotModel{};
  vertexBufferViewTeapotModel.BufferLocation =
      vertexResourceTeapotModel
          ->GetGPUVirtualAddress(); // リソースの先頭のアドレスからバッファ
  vertexBufferViewTeapotModel.SizeInBytes =
      UINT(sizeof(VertexData) *
           teapotModel.vertices.size()); // 頂点リソースは頂点のサイズ
  vertexBufferViewTeapotModel.StrideInBytes =
      sizeof(VertexData); // 1頂点あたりのサイズ

  // 頂点リソースにデータを書き込む
  VertexData *vertexDataTeapotModel = nullptr;
  vertexResourceTeapotModel->Map(
      0, nullptr,
      reinterpret_cast<void **>(
          &vertexDataTeapotModel)); // 書き込むためのアドレスを取得
  std::memcpy(vertexDataTeapotModel, teapotModel.vertices.data(),
              sizeof(VertexData) *
                  teapotModel.vertices.size()); // 頂点データをリソースにコピー

  //=========================
  // bunnyModel用の頂点リソースを作る
  //=========================

  ModelData bunnyModel = LoadObjFile("Resources", "bunny.obj");

  // モデル用テクスチャを読み込む
  DirectX::ScratchImage mipImagesBunnyModel =
      LoadTexture(bunnyModel.material.textureFilePath);
  const DirectX::TexMetadata &metadataBunnyModel = mipImagesBunnyModel.GetMetadata();
  ID3D12Resource *textureResourceBunnyModel =
      CreateTextureResource(device, metadataBunnyModel);
  UploadTextureData(textureResourceBunnyModel, mipImagesBunnyModel);

  // 頂点リソースを作る
  ID3D12Resource *vertexResourceBunnyModel = CreateBufferResource(
      device, sizeof(VertexData) * bunnyModel.vertices.size());
  // 頂点バッファビューを作成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewBunnyModel{};
  vertexBufferViewBunnyModel.BufferLocation =
      vertexResourceBunnyModel
          ->GetGPUVirtualAddress(); // リソースの先頭のアドレスからバッファ
  vertexBufferViewBunnyModel.SizeInBytes =
      UINT(sizeof(VertexData) *
           bunnyModel.vertices.size()); // 頂点リソースは頂点のサイズ
  vertexBufferViewBunnyModel.StrideInBytes =
      sizeof(VertexData); // 1頂点あたりのサイズ

  // 頂点リソースにデータを書き込む
  VertexData *vertexDataBunnyModel = nullptr;
  vertexResourceBunnyModel->Map(
      0, nullptr,
      reinterpret_cast<void **>(
          &vertexDataBunnyModel)); // 書き込むためのアドレスを取得
  std::memcpy(vertexDataBunnyModel, bunnyModel.vertices.data(),
              sizeof(VertexData) *
                  bunnyModel.vertices.size()); // 頂点データをリソースにコピー



  // SRVを作成
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDescModel = {};
  srvDescModel.Format = metadataModel.format;
  srvDescModel.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDescModel.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDescModel.Texture2D.MipLevels = UINT(metadataModel.mipLevels);

  D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPUModel =
      GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPUModel =
      GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);

  device->CreateShaderResourceView(textureResourceModel, &srvDescModel,
                                   textureSrvHandleCPUModel);

  Transform modelTransform{
      {1.0f, 1.0f, 1.0f}, // スケール
      {0.0f, 0.0f, 0.0f}, // 回転
      {0.0f, 0.0f, 0.0f}  // 位置
  };

  // --- Model用のCBVリソースを作成 ---
  ID3D12Resource *modelWvpResource =
      CreateBufferResource(device, sizeof(TransformationMatrix));
  TransformationMatrix *modelWvpData = nullptr;
  modelWvpResource->Map(0, nullptr, reinterpret_cast<void **>(&modelWvpData));
  modelWvpData->WVP = MakeIdentity4x4();   // 単位行列で初期化
  modelWvpData->World = MakeIdentity4x4(); // 単位行列で初期化
  // --- Model用のMaterialリソースを作成 ---
  ID3D12Resource *modelMaterialResource =
      CreateBufferResource(device, sizeof(Material));
  Material *modelMaterialData = nullptr;
  modelMaterialResource->Map(0, nullptr,
                             reinterpret_cast<void **>(&modelMaterialData));
  // Materialの初期化
  modelMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白色
  modelMaterialData->uvTransform =
      MakeIdentity4x4();               // UV変換行列を単位行列で初期化
  modelMaterialData->lightingMode = 2; // 0:なし, 1:Lambert, 2:Half Lambert

  // --- DirectionalLight用のCBVリソースを作成 ---
  ID3D12Resource *modelDirectionalLightResource =
      CreateBufferResource(device, sizeof(DirectionalLight));
  DirectionalLight *modelDirectionalLightData = nullptr;
  modelDirectionalLightResource->Map(
      0, nullptr, reinterpret_cast<void **>(&modelDirectionalLightData));
  *modelDirectionalLightData = directionalLight; // 初期値をコピー

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
  ImGui_ImplDX12_Init(device, swapChainDesc.BufferCount, rtvDesc.Format,
                      srvDescriptorHeap,
                      srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                      srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

  //===============================
  // Textureをよんで転送する
  //===============================
  // 1枚目のTextureを読んで転送する
  DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
  const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
  ID3D12Resource *textureResource = CreateTextureResource(device, metadata);
  UploadTextureData(textureResource, mipImages);

  // metadataを元にSRVを生成する
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = metadata.format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

  // SRVを作成するDescriptorHeapの場所を決める
  ::D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
      srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

  ::D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
      srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

  textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // SRVを生成する
  device->CreateShaderResourceView(textureResource, &srvDesc,
                                   textureSrvHandleCPU);

  // 2枚目のTextureを読んで転送する
  DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
  const DirectX::TexMetadata metadata2 = mipImages2.GetMetadata();
  ID3D12Resource *textureResource2 = CreateTextureResource(device, metadata2);
  UploadTextureData(textureResource2, mipImages2);

  // metadataを元にSRVを生成する
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2 = {};
  srvDesc2.Format = metadata2.format;
  srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);
  // SRVを作成するDescriptorHeapの場所を決める
  ::D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 =
      srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  ::D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
      srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
  textureSrvHandleCPU2.ptr += 2 * device->GetDescriptorHandleIncrementSize(
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  textureSrvHandleGPU2.ptr += 2 * device->GetDescriptorHandleIncrementSize(
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  // SRVを生成する
  device->CreateShaderResourceView(textureResource2, &srvDesc2,
                                   textureSrvHandleCPU2);

  // 3枚目のTextureを読んで転送する
  DirectX::ScratchImage mipImages3 = LoadTexture("Resources/checkerBoard.png");
  const DirectX::TexMetadata metadata3 = mipImages3.GetMetadata();
  ID3D12Resource *textureResource3 = CreateTextureResource(device, metadata3);
  UploadTextureData(textureResource3, mipImages3);

  // metadataを元にSRVを生成する
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3 = {};
  srvDesc3.Format = metadata3.format;
  srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);
  // SRVを作成するDescriptorHeapの場所を決める
  ::D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 =
      srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  ::D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 =
      srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
  textureSrvHandleCPU3.ptr += 3 * device->GetDescriptorHandleIncrementSize(
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  textureSrvHandleGPU3.ptr += 3 * device->GetDescriptorHandleIncrementSize(
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  // SRVを生成する
  device->CreateShaderResourceView(textureResource3, &srvDesc3,
                                   textureSrvHandleCPU3);

  // 0: uvChecker, 1: モンスターボール
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

            const char *triangleTextureItems[] = {"uvChecker", "モンスターボール", "checkerBoard"};
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
                  ImGui::SliderFloat3("##Translation",
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
                  ImGui::SliderFloat3("##Scale", &transformSprite.scale.x, 0.0f,
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

            const char *sphereTextureItems[] = {"uvChecker", "モンスターボール",
                                                "checkerBoard"};
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

            const char *modelItems[] = {"Plane", "axis", "ティーポット", "バニー"};
            ImGui::Combo("モデル選択", &modelIndex, modelItems,
                         IM_ARRAYSIZE(modelItems));

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            const char *modelTextureItems[] = {"uvChecker", "モンスターボール",
                                               "checkerBoard"};
            ImGui::Combo("テクスチャ選択", &modelTextureIndex,
                         modelTextureItems, IM_ARRAYSIZE(modelTextureItems));

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            if (ImGui::BeginTabBar("設定")) {

              if (ImGui::BeginTabItem("3Dモデル設定")) {
                if (ImGui::CollapsingHeader("位置",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Translation",
                                      &modelTransform.translation.x, -10.0f,
                                      10.0f);
                  if (ImGui::Button("位置のリセット")) {
                    modelTransform.translation = {0.0f, 0.0f, 0.0f};
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("回転",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Rotation", &modelTransform.rotation.x,
                                      -10.0f, 10.0f);
                  if (ImGui::Button("回転のリセット")) {
                    modelTransform.rotation = {0.0f, 0.0f, 0.0f};
                  }
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::CollapsingHeader("大きさ",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::SliderFloat3("##Scale", &modelTransform.scale.x, 0.1f,
                                      10.0f);
                  if (ImGui::Button("大きさのリセット")) {
                    modelTransform.scale = {1.0f, 1.0f, 1.0f};
                  }
                }

                ImGui::EndTabItem(); // 3Dモデル - 設定タブを終了
              }

              if (ImGui::BeginTabItem("マテリアル設定")) {
                if (ImGui::CollapsingHeader("色",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                  ImGui::ColorEdit3("##Color", &modelMaterialData->color.x);
                  if (ImGui::Button("色のリセット")) {
                    modelMaterialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                  }
                }
                ImGui::EndTabItem(); // マテリアル設定タブを終了
              }

              if (ImGui::BeginTabItem("ライト設定")) {
                if (ImGui::CollapsingHeader("Lighting",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {

                  // ImGuiで切り替えUI
                  ImGui::Combo("ライティングモード",
                               &modelMaterialData->lightingMode,
                               "無し\0ランバート反射\0ハーフランバート\0");

                  ImGui::Dummy(ImVec2(0.0f, 5.0f));

                  if (modelMaterialData->lightingMode != 0) {

                    if (ImGui::CollapsingHeader(
                            "ライトの色", ImGuiTreeNodeFlags_DefaultOpen)) {
                      ImGui::ColorEdit3("##Light Color",
                                        &modelDirectionalLightData->color.x);

                      if (ImGui::Button("ライトの色をリセット")) {
                        modelDirectionalLightData->color =
                            Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                      }
                    }

                    ImGui::Dummy(ImVec2(0.0f, 5.0f));

                    if (ImGui::CollapsingHeader(
                            "ライトの向き", ImGuiTreeNodeFlags_DefaultOpen)) {
                      ImGui::SliderFloat3(
                          "##Light Direction",
                          &modelDirectionalLightData->direction.x, -1.0f, 1.0f);

                      if (ImGui::Button("向きのリセット")) {
                        modelDirectionalLightData->direction = {0.0f, -1.0f,
                                                                0.0f};
                      }
                    }

                    ImGui::Dummy(ImVec2(0.0f, 5.0f));

                    if (ImGui::CollapsingHeader(
                            "ライトの強さ", ImGuiTreeNodeFlags_DefaultOpen)) {

                      ImGui::SliderFloat("##Intensity",
                                         &modelDirectionalLightData->intensity,
                                         0.0f, 1.0f, "%.2f");
                      if (ImGui::Button("ライトの強さをリセット")) {
                        modelDirectionalLightData->intensity = 1.0f;
                      }
                    }

                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                  }
                }
                ImGui::EndTabItem(); // ライト設定タブを終了
              }
              ImGui::EndTabBar(); // 3Dモデル - 設定タブを終了
            }
          }
          ImGui::EndTabItem(); // 3Dモデルタブを終了
        }

        if (ImGui::BeginTabItem("サウンド")) { // サウンドタブを開始

          Alarm01.SoundImGui("Alarm01");
          Alarm02.SoundImGui("Alarm02");
          fanfare.SoundImGui("Fanfare");

          ImGui::EndTabItem(); // サウンドタブを終了
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
      } else {
        ImGui::Text("メインカメラモード");
      }

      ImGui::End(); // 各種設定ウィンドウを終了

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

      // ログの出力
      // Log(ofs, "ウィンドウが表示されました");
      // これから書き込むバックバッファのインデックスを取得
      UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

      // 指定した色でクリアする
      float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};

      // TransitionBarrierを設定する
      D3D12_RESOURCE_BARRIER barrier{};
      // 今回のバリアはトランジションバリア
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      // Noneにしておく
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      // バリアを張る対象のリソース。現在のバックバッファに対して行う
      barrier.Transition.pResource = swapChainResource[backBufferIndex];
      // 遷移前のResourceState
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      // 遷移後のResourceState
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      // TransitionBarrierを振る
      commandList->ResourceBarrier(1, &barrier);

      // 描画先のRTVを設定
      commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false,
                                      &dsvHandle);
      commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
                                         clearColor, 0, nullptr);
      commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
                                         1.0f, 0, 0, nullptr);

      ID3D12DescriptorHeap *descriptorHeaps[] = {srvDescriptorHeap};

      commandList->SetDescriptorHeaps(1, descriptorHeaps);

      //============================
      // コマンドを積む 02_00
      //============================

      commandList->RSSetViewports(1, &viewport);       // Viewportを設定する
      commandList->RSSetScissorRects(1, &scissorRect); // Scissorを設定する
      // RootSignatureを設定。PSOに設定しているけれど別途設定が必要
      commandList->SetGraphicsRootSignature(rootSignature);
      commandList->SetPipelineState(graphicsPipelineState);     // PSOを設定する
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

      //===========================================
      // SRVのDescriptorTableを設定する 03_00
      //===========================================

      D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
          GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

      // SRVディスクリプタヒープの先頭GPUハンドルを取得
      D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
          GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

      // 二枚目のテクスチャのSRVを設定する
      D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 =
          GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

      D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
          GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

      device->CreateShaderResourceView(textureResource2, &srvDesc2,
                                       textureSrvHandleCPU2);

      if (isTriangle1) {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        // Material用CBVをセット
        commandList->SetGraphicsRootConstantBufferView(
            0, triangleMaterialResource->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(
            3, directionalLightResource->GetGPUVirtualAddress());
        if (triangleTextureIndex == 0) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU); // uvChecker
        } else if (triangleTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU2); // モンスターボール
        } else if (triangleTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU3); // checkerBoard
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
              2, textureSrvHandleGPU); // uvChecker
        } else if (triangleTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU2); // モンスターボール
        } else if (triangleTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU3); // checkerBoard
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
              2, textureSrvHandleGPU); // uvChecker
        } else if (sphereTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU2); // モンスターボール
        } else if (sphereTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU3); // checkerBoard
        }
        // DirectionalLight用CBV
        commandList->SetGraphicsRootConstantBufferView(
            3, directionalLightResource->GetGPUVirtualAddress());

        sphere->Draw(commandList);
      }

      if (isModelPlane) {

        // Model用のMaterialのCBVを設定
        commandList->SetGraphicsRootConstantBufferView(
            0, modelMaterialResource->GetGPUVirtualAddress());
        // Model用のWVPのCBVを設定
        commandList->SetGraphicsRootConstantBufferView(
            1, modelWvpResource->GetGPUVirtualAddress());

        ModelData *currentModelData =
            (modelIndex == 0) ? &planeModel : &axisModel;

        if (modelTextureIndex == 0) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU); // uvChecker
        } else if (modelTextureIndex == 1) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU2); // モンスターボール
        } else if (modelTextureIndex == 2) {
          commandList->SetGraphicsRootDescriptorTable(
              2, textureSrvHandleGPU3); // checkerBoard
        }

        // DirectionalLight用CBVを設定（追加）
        commandList->SetGraphicsRootConstantBufferView(
            3, modelDirectionalLightResource->GetGPUVirtualAddress());

        // モデル用の頂点バッファをセット
        if (modelIndex == 0) {
          // Plane
          commandList->IASetVertexBuffers(0, 1, &vertexBufferViewPlaneModel);
          commandList->DrawInstanced(UINT(planeModel.vertices.size()), 1, 0, 0);
        } else if (modelIndex == 1) {
          // Axis
          commandList->IASetVertexBuffers(0, 1, &vertexBufferViewAxisModel);
          commandList->DrawInstanced(UINT(axisModel.vertices.size()), 1, 0, 0);
        } else if (modelIndex == 2) {
          // ティーポット
          commandList->IASetVertexBuffers(0, 1, &vertexBufferViewTeapotModel);
          commandList->DrawInstanced(UINT(teapotModel.vertices.size()), 1, 0,
                                     0);
        } else if (modelIndex == 3) {
          // バニー
          commandList->IASetVertexBuffers(0, 1, &vertexBufferViewBunnyModel);
          commandList->DrawInstanced(UINT(bunnyModel.vertices.size()), 1, 0, 0);
        }
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
            2, textureSrvHandleGPU); // テクスチャのSRVを設定

        // 常にuvCheckerを表示する
        commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

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

      // 画面に各処理は全て終わり、画面に映すので、状態を遷移
      // 今回はRenderTargetからPresentにする
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      // TransitionBarrierを張る
      commandList->ResourceBarrier(1, &barrier);

      // コマンドリストの内容を確定させる。全てのコマンドを詰んでからCloseする
      hr = commandList->Close();
      // コマンドリストの確定に失敗した場合はエラー
      assert(SUCCEEDED(hr));

      // GPUにコマンドリストの実行を行わせる
      ID3D12CommandList *commandLists[] = {commandList};
      commandQueue->ExecuteCommandLists(1, commandLists);
      // GPUとOSに画面の交換を行わせる
      swapChain->Present(1, 0);

      // GPUにSignalを送る
      // Fenceの値を更新
      fenceValue++;
      // GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
      commandQueue->Signal(fence, fenceValue);

      // Fenceの値が指定した値になるまで待つ
      // GetCompletedValueの初期値はFence作成時に渡した初期値
      if (fence->GetCompletedValue() < fenceValue) {
        // 指定したSangalの値にたどりついていないので、たどり着くまで待つようにイベントを設定する
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        // Signalを受け取るまで待つ
        WaitForSingleObject(fenceEvent, INFINITE);
      }

      // 次のフレーム用のコマンドリストを準備
      hr = commandAllocator->Reset();
      // コマンドアロケータのリセットに失敗した場合はエラー
      assert(SUCCEEDED(hr));
      hr = commandList->Reset(commandAllocator, nullptr);
      // コマンドリストのリセットに失敗した場合はエラー
      assert(SUCCEEDED(hr));

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

      // --- Sphere用の更新 ---

      if (eanableSphereRotateY) {
        sphere->sphereTransform.rotation.y += 0.001f;
      }

      Matrix4x4 sphereWorldMatrix = MakeAffineMatrix(
          sphere->sphereTransform.scale, sphere->sphereTransform.rotation,
          sphere->sphereTransform.translation);

      Matrix4x4 sphereWvpMatrix =
          Multiply(sphereWorldMatrix, Multiply(view, proj));

      sphereWvpData->WVP = sphereWvpMatrix;
      sphereWvpData->World = sphereWorldMatrix;

      *directionalLightData = directionalLight;

      // --- modelの更新 ---

      Matrix4x4 modelWorldMatrix =
          MakeAffineMatrix(modelTransform.scale, modelTransform.rotation,
                           modelTransform.translation);

      Matrix4x4 modelWvpMatrix =
          Multiply(modelWorldMatrix, Multiply(view, proj));

      modelWvpData->WVP = modelWvpMatrix;
      modelWvpData->World = modelWorldMatrix;
    }
  }

  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  //===========================
  // 解放処理
  //===========================

  vertexResource->Release();
  vertexResourceSprite->Release();
  transformationMatrixResourceSprite->Release();
  depthStencilResource->Release();
  dsvDescriptorHeap->Release();
  sphereWvpResource->Release();

  graphicsPipelineState->Release();
  signatureBlob->Release();
  if (errorBlob) {
    errorBlob->Release();
  }
  rootSignature->Release();
  pixelShaderBlob->Release();
  vertexShaderBlob->Release();

  srvDescriptorHeap->Release();
  rtvDescriptorHeap->Release();
  swapChainResource[0]->Release();
  swapChainResource[1]->Release();
  swapChain->Release();
  commandList->Release();
  commandAllocator->Release();
  commandQueue->Release();
  fence->Release();
  wvpResource->Release();
  textureResource->Release();
  dxcUtils->Release();
  dxcCompiler->Release();
  includeHandler->Release();
  dxgiFactory->Release();
  useAdapter->Release();
  device->Release();

  delete sphere;

  // COMの終了処理
  CoUninitialize();

  return 0;
}
