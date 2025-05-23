#pragma warning(push)
#pragma warning(disable : 28251)
#include "mySource/Affine/Affine.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "mySource/struct.h"
#include <Windows.h>
#include <cassert>
#include <chrono> //時間を扱うライブラリ
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <filesystem> //ファイルやディレクトリの操作を行うライブラリ
#include <format>     //文字列のフォーマットを行うライブラリ
#include <fstream>    //ファイル入出力を行うライブラリ
#include <string>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma warning(pop)

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd,
                                                             UINT msg,
                                                             WPARAM wparam,
                                                             LPARAM lparam);

// ウィンドウサイズを表す構造体にクライアント領域を入れる
RECT wrc = {0, 0, kClientWidth, kClientHeight};

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true;
  }

  // メッセージに応じてゲーム固有の処理を行う
  switch (msg) {
    // ウィンドウが破棄された
  case WM_DESTROY:
    // OSに対して、アプリの終了を伝える
    PostQuitMessage(0);
    return 0;
  }

  // 標準のメッセージ処理を行う
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

// ログをファイルに書き出す
void Log(std::ostream &os, const std::string &message) {
  // ログファイルにメッセージを書き込む
  os << message << std::endl;
  // 出力ウィンドウにもメッセージを書き込む
  OutputDebugStringA(message.c_str());
}

void Log(const std::string &message) { OutputDebugStringA(message.c_str()); }

// stringをwstringに変換する関数
std::wstring ConvertString(const std::string &str) {
  // stringのサイズを取得
  int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  // wstringのサイズを取得
  std::wstring wstr(size, L'\0');
  // stringをwstringに変換
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
  return wstr;
}

// wstringをstringに変換する関数
std::string ConvertString(const std::wstring &wstr) {
  // wstringのサイズを取得
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
                                 nullptr, nullptr);
  // stringのサイズを取得
  std::string str(size, '\0');
  // wstringをstringに変換
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr,
                      nullptr);
  return str;
}

IDxcBlob *CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring &filePath,
    // CompilerにしようするProfile
    const wchar_t *profile,
    // 初期化で生成したものを3つ
    IDxcUtils *dxcUtils, IDxcCompiler3 *dxcCompiler,
    IDxcIncludeHandler *includeHandler) {
  // 　これからシェーダーをコンパイルする旨をログに出力
  Log(ConvertString(std::format(L"Begin CompileShader, path:{},profile: {}\n",
                                filePath, profile)));
  //================================
  // hlslファイルを読む
  //================================

  IDxcBlobEncoding *shaderSource = nullptr;
  HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
  // 読み込めなかったらエラー
  assert(SUCCEEDED(hr));
  // 読み込んだファイルの内容を設定する
  DxcBuffer shaderSourceBuffer;
  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8;

  //================================
  // コンパイルする
  //================================

  LPCWSTR arguments[] = {
      filePath.c_str(), // コンパイル対象のhlslファイル名
      L"-E",
      L"main", // エントリーポイントの指定。基本的にmain以外にはしない
      L"-T",
      profile, // ShaderProfileの設定
      L"-Zi",
      L"-Qembed_debug", // デバッグ用の情報を埋め込む
      L"-Od",           // 最適化をしない
      L"-Zpr",          // メモリレイアウトは行優先
  };

  // 実際にShaderをコンパイルする
  IDxcResult *shaderResult = nullptr;
  hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments, _countof(arguments),
                            includeHandler, IID_PPV_ARGS(&shaderResult));
  // コンパイルに失敗したらエラー
  assert(SUCCEEDED(hr));

  //================================
  // 警告・エラーを確認する
  //================================

  // 警告・エラーが出てたらログに出力して止める
  IDxcBlobUtf8 *shaderError = nullptr;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
  if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
    Log(shaderError->GetStringPointer());
    // 警告・エラーだめ絶対
    assert(false);
  }

  //================================
  // コンパイル結果を取得して返す
  //================================

  // コンパイル結果から実行用のバイナリ部分を取得する
  IDxcBlob *shaderBlob = nullptr;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
                               nullptr);
  assert(SUCCEEDED(hr));

  // 成功したログを出す
  Log(ConvertString(std::format(L"Compile Suceeded, path:{},profile: {}\n",
                                filePath, profile)));
  // もう使わないリソースを解放
  shaderSource->Release();
  shaderResult->Release();

  // 実行用のバイナリを返す
  return shaderBlob;
}

// Resource作成関数
ID3D12Resource *CreateBufferResource(ID3D12Device *device, size_t sizeInBytes) {
  // ヒーププロパティの設定
  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード用ヒープ

  // リソースの設定
  D3D12_RESOURCE_DESC bufferDesc{};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = sizeInBytes; // リソースのサイズ
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  // リソースを作成
  ID3D12Resource *buffer = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties,                   // ヒーププロパティ
      D3D12_HEAP_FLAG_NONE,              // ヒープフラグ
      &bufferDesc,                       // リソースの設定
      D3D12_RESOURCE_STATE_GENERIC_READ, // 初期状態
      nullptr,                           // クリア値 (バッファの場合は不要)
      IID_PPV_ARGS(&buffer));            // 作成されたリソースを受け取る

  // 作成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  return buffer;
};

ID3D12DescriptorHeap *CreateDescriptorHeap(ID3D12Device *device,
                                           D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                           UINT numDescriptors,
                                           bool shaderVisible) {
  ID3D12DescriptorHeap *descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
  descriptorHeapDesc.Type = heapType;
  descriptorHeapDesc.NumDescriptors = numDescriptors;
  descriptorHeapDesc.Flags = shaderVisible
                                 ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,
                                            IID_PPV_ARGS(&descriptorHeap));
  assert(SUCCEEDED(hr));
  return descriptorHeap;
}

DirectX::ScratchImage LoadTexture(const std::string &filePath) {
  // テクスチャを読み込む
  DirectX::ScratchImage image{};
  std::wstring filePathW = ConvertString(filePath);
  HRESULT hr = DirectX::LoadFromWICFile(
      filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
  // 読み込みに失敗したらエラー
  assert(SUCCEEDED(hr));

  // ミニマップの作成
  DirectX::ScratchImage mipImages{};
  hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                                image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
                                0, mipImages);
  // ミニマップの作成に失敗したらエラー
  assert(SUCCEEDED(hr));

  // ミニマップを返す
  return mipImages;
}

ID3D12Resource *CreateTextureResource(ID3D12Device *device,
                                      const DirectX::TexMetadata &metadata) {
  // metadataを基にResourceの設定

  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = UINT(metadata.width);
  resourceDesc.Height = UINT(metadata.height);
  resourceDesc.MipLevels = UINT16(metadata.mipLevels);
  resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
  resourceDesc.Format = metadata.format;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

  // 利用するヒープの設定

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // カスタムヒープ
  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

  // Resourceを生成する
  ID3D12Resource *resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));

  // Resourceの生成に失敗したらエラー
  assert(SUCCEEDED(hr));

  return resource;
}

void UploadTextureData(ID3D12Resource *texture,
                       const DirectX::ScratchImage &mipImages) {
  // Metadataを取得
  const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
  // 全MipMapについて
  for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {

    const DirectX::Image *img = mipImages.GetImage(mipLevel, 0, 0);

    HRESULT hr =
        texture->WriteToSubresource(UINT(mipLevel), nullptr, img->pixels,
                                    UINT(img->rowPitch), UINT(img->slicePitch));
    // 書き込みに失敗したらエラー
    assert(SUCCEEDED(hr));
  }
}

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

  // COMライブラリの初期化
  CoInitializeEx(0, COINIT_MULTITHREADED);
  // COMライブラリの初期化に失敗したらエラー
  HRESULT hrCoInt = CoInitializeEx(0, COINIT_MULTITHREADED);

  // ログのディレクトリを用意
  std::filesystem::create_directory("logs");
  // 現在の時間を取得(UTC)
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  // ログファイルの名前にコンマ何秒はいらないので、削って秒にする
  std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
      nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  // 日本時間に変換
  std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};
  // formatを使って年月日_時分秒の文字列に変換
  std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
  // 時間を使ってファイル名を決定
  std::string logFilePath = std::string("logs/") + dateString + ".log";
  // ファイルを作って書き込み準備
  std::ofstream ofs(logFilePath);

  WNDCLASS wc{};
  // ウィンドウプロシージャ
  wc.lpfnWndProc = WindowProc;
  // ウィンドウクラス名
  wc.lpszClassName = L"CG2WindowClass";
  // インスタンスハンドル
  wc.hInstance = GetModuleHandle(nullptr);
  // カーソル
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  // ウィンドウクラスの登録
  RegisterClass(&wc);

  // クライアント領域を元に実際のサイズにwrcを変更してもらう
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

  // ウィンドウの生成
  HWND hwnd = CreateWindow(wc.lpszClassName,     // ウィンドウクラス名
                           L"CG2",               // ウィンドウ名
                           WS_OVERLAPPEDWINDOW,  // ウィンドウスタイル
                           CW_USEDEFAULT,        // x座標
                           CW_USEDEFAULT,        // y座標
                           wrc.right - wrc.left, // 幅
                           wrc.bottom - wrc.top, // 高さ
                           nullptr,              // 親ウィンドウハンドル
                           nullptr,              // メニューハンドル
                           wc.hInstance,         // インスタンスハンドル
                           nullptr);             // オプション

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
      Log(ConvertString(
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
      Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
      break;
    }
  }
  // デバイスが生成できなかった場合はエラー
  assert(device != nullptr);
  Log("Complete create D3D12Device!!!\n"); // 初期化完了のログ出力

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
      commandQueue, hwnd, &swapChainDesc, nullptr, nullptr,
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

  typedef struct D3D12_CPU_DESCRIPTOR_HANDLE {
    SIZE_T ptr;
  } D3D12_CPU_DESCRIPTOR_HANDLE;

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
  D3D12_ROOT_PARAMETER rootParameters[3] = {};
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
    Log(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
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

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
  inputElementDescs[0].SemanticName = "POSITION"; // セマンティクス名
  inputElementDescs[0].SemanticIndex = 0;         // セマンティクスインデックス
  inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  inputElementDescs[1].SemanticName = "TEXCOORD"; // セマンティクス名
  inputElementDescs[1].SemanticIndex = 0;         // セマンティクスインデックス
  inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

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
  // 実際に生成
  ID3D12PipelineState *graphicsPipelineState = nullptr;
  hr = device->CreateGraphicsPipelineState(
      &graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
  // PSOの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

  //==========================
  // VertexResourceを生成する
  //==========================

  ID3D12Resource *vertexResource =
      CreateBufferResource(device, sizeof(Vector4) * 3);

  //===========================
  // VertexBufferViewを生成する
  //===========================
  // 頂点バッファビューを生成する
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
  // リソースの先頭のアドレスから使う
  vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
  // 使用するリソースのサイズは頂点3つ分のサイズ
  vertexBufferView.SizeInBytes = sizeof(VertexData) * 3;
  // 1つの頂点のサイズ
  vertexBufferView.StrideInBytes = sizeof(VertexData);

  //===========================
  // Resourceにデータを転送する
  //===========================

  VertexData *vertexData = nullptr;

  vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

  // 左下
  vertexData[0].Position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
  vertexData[0].Texcoord = Vector2(0.0f, 1.0f);

  // 上
  vertexData[1].Position = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
  vertexData[1].Texcoord = Vector2(0.5f, 0.0f);

  // 右下
  vertexData[2].Position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
  vertexData[2].Texcoord = Vector2(1.0f, 1.0f);

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
  // Material用のResourceを生成する
  //===============================
  // マテリアル用のリソースを作る
  ID3D12Resource *materialResource =
      CreateBufferResource(device, sizeof(Vector4));
  // マテリアル用のリソースにデータを書き込む
  Vector4 *materialData = nullptr;
  // 書き込むためのアドレスを取得
  materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
  // マテリアルの色を設定
  *materialData = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

  //=================================================
  // TransformationMatrix用のResourceを生成する 02_02
  //=================================================

  ID3D12Resource *wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
  // データを書き込む
  Matrix4x4 *wvpData = nullptr;
  // 書き込むためのアドレスを取得
  wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));
  // 単位行列を書き込んでおく
  *wvpData = MakeIdentity4x4();

  //===============================
  // Transform変数を作る
  //===============================

  Transform transform{
      {1.0f, 1.0f, 1.0f}, // 位置
      {0.0f, 0.0f, 0.0f}, // 回転
      {0.0f, 0.0f, 0.0f}, // スケール
  };

  Transform cameraTransform{
      {1.0f, 1.0f, 1.0f},  // 位置
      {0.0f, 0.0f, 0.0f},  // 回転
      {0.0f, 0.0f, -5.0f}, // スケール
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

  //===============================
  // Imguiの初期化
  //===============================

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(device, swapChainDesc.BufferCount, rtvDesc.Format,
                      srvDescriptorHeap,
                      srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                      srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

  //===============================
  // Textureをよんで転送する
  //===============================
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

      // ImGui::ShowDemoWindow(); // デモウィンドウの表示
      ImGui::Begin("Triangle Setting");
      if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 色を操作するカラーピッカー
        ImGui::ColorEdit3("Color", color);
        // 色をリセットするボタン
        if (ImGui::Button("Reset Color")) {
          color[0] = 1.0f;
          color[1] = 1.0f;
          color[2] = 1.0f;
          color[3] = 1.0f;
        }
      }

      if (ImGui::CollapsingHeader("Translation",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Transform Translation");
        ImGui::SliderFloat3("Translation", &transform.translation.x, -2.0f,
                            2.0f);
        if (ImGui::Button("Reset Translation")) {
          transform.translation = {0.0f, 0.0f, 0.0f};
        }
      }

      if (ImGui::CollapsingHeader("Rotation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Transform Rotation");
        ImGui::SliderFloat3("Rotation", &transform.rotation.x, -2.0f, 2.0f);
        if (ImGui::Button("Reset Rotation")) {
          transform.rotation = {0.0f, 0.0f, 0.0f};
        }
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20.0f, 0.0f)); // 20ピクセル分の空白
        ImGui::SameLine();
        ImGui::Checkbox("RotateX", &enableRotateX);
        ImGui::SameLine();
        ImGui::Checkbox("RotateY", &enableRotateY);
        ImGui::SameLine();
        ImGui::Checkbox("RotateZ", &enableRotateZ);
      }

      if (ImGui::CollapsingHeader("Scale", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Transform Scale");
        ImGui::SliderFloat3("Scale", &transform.scale.x, 0.0f, 4.0f);
        if (ImGui::Button("Reset Scale")) {
          transform.scale = {1.0f, 1.0f, 1.0f};
        }
      }

      ImGui::End();

      // ウィンドウの表示
      ShowWindow(hwnd, SW_SHOW);
      // ログの出力
      // Log(ofs, "ウィンドウが表示されました");
      // これから書き込むバックバッファのインデックスを取得
      UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
      // 描画先のRTVを設定
      commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false,
                                      nullptr);

      *materialData = Vector4(color[0], color[1], color[2], color[3]);
      // 指定した色でクリアする
      float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
      commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
                                         clearColor, 0, nullptr);

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
          0, materialResource->GetGPUVirtualAddress());

      //===========================================
      // TransformationMatrix用のCBVを設定する 02_02
      //===========================================
      commandList->SetGraphicsRootConstantBufferView(
          1, wvpResource->GetGPUVirtualAddress());

      //===========================================
      // SRVのDescriptorTableを設定する 03_00
      //===========================================

      // SRVディスクリプタヒープの先頭GPUハンドルを取得
      D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
          srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

      commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

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

      D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU =
          srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
      srvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(
          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      commandList->SetGraphicsRootDescriptorTable(2, srvHandleGPU);

      ImGui::Render();

      //===========--==============================
      //
      // !!!!!!描画!!!!
      //
      //===========================================

      commandList->DrawInstanced(3, 1, 0, 0); // 3頂点を描画する
      ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

      // コマンドリストの内容を確定させる。全てのコマンドを詰んでからCloseする
      hr = commandList->Close();
      // コマンドリストの確定に失敗した場合はエラー
      assert(SUCCEEDED(hr));

      // 画面に各処理は全て終わり、画面に映すので、状態を遷移
      // 今回はRenderTargetからPresentにする
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      // TransitionBarrierを張る
      commandList->ResourceBarrier(1, &barrier);

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

      if (enableRotateX) {
        transform.rotation.x += 0.03f;
      }
      if (enableRotateY) {
        transform.rotation.y += 0.03f;
      }
      if (enableRotateZ) {
        transform.rotation.z += 0.03f;
      }

      Matrix4x4 worldMatrix = MakeAffineMatrix(
          transform.scale, transform.rotation, transform.translation);

      Matrix4x4 cameraMatrix =
          MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotation,
                           cameraTransform.translation);

      Matrix4x4 viewMatrix = Inverse(cameraMatrix);

      Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
          0.45f,
          static_cast<float>(kClientWidth) / static_cast<float>(kClientHeight),
          0.1f, 100.0f);

      Matrix4x4 wvpMatrix =
          Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

      *wvpData = wvpMatrix;
    }
  }

  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  //===========================
  // 解放処理
  //===========================

  vertexResource->Release();
  graphicsPipelineState->Release();
  signatureBlob->Release();
  if (errorBlob) {
    errorBlob->Release();
  }
  rootSignature->Release();
  pixelShaderBlob->Release();
  vertexShaderBlob->Release();
  materialResource->Release();

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

  // COMの終了処理
  CoUninitialize();

  return 0;
}
