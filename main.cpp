#pragma warning(push)
#pragma warning(disable : 28251)
#include <Windows.h>
#include <cassert>
#include <chrono> //時間を扱うライブラリ
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <filesystem> //ファイルやディレクトリの操作を行うライブラリ
#include <format>     //文字列のフォーマットを行うライブラリ
#include <fstream>    //ファイル入出力を行うライブラリ
#include <string>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma warning(pop)

// クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

// ウィンドウサイズを表す構造体にクライアント領域を入れる
RECT wrc = {0, 0, kClientWidth, kClientHeight};

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
std::wstring ConverString(const std::string &str) {
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

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
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
       //Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
    };
    //抑制するレベル
    D3D12_MESSAGE_SEVERITY severities[] = {
        D3D12_MESSAGE_SEVERITY_INFO
    };
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
  ID3D12DescriptorHeap *rtvDescriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
  rtvDescriptorHeapDesc.Type =
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV;       // レンダーターゲットビュー用
  rtvDescriptorHeapDesc.NumDescriptors = 2; // スワップチェーンの数と同じ
  hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
                                    IID_PPV_ARGS(&rtvDescriptorHeap));
  // ディスクリプタヒープの生成に失敗した場合はエラー
  assert(SUCCEEDED(hr));

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

  // ウィンドウのxボタンが押されるまでループ
  while (msg.message != WM_QUIT) {
    // メッセージがある場合は、メッセージを取得
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      // メッセージを処理
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // ゲームの処理

      // ウィンドウの表示
      ShowWindow(hwnd, SW_SHOW);
      // ログの出力
      // Log(ofs, "ウィンドウが表示されました");
      // これから書き込むバックバッファのインデックスを取得
      UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
      // 描画先のRTVを設定
      commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false,
                                      nullptr);
      // 指定した色でクリアする
      float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
      commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
                                         clearColor, 0, nullptr);
      // コマンドリストの内容を確定させる。全てのコマンドを詰んでからCloseする
      hr = commandList->Close();
      // コマンドリストの確定に失敗した場合はエラー
      assert(SUCCEEDED(hr));

      // GPUにコマンドリストの実行を行わせる
      ID3D12CommandList *commandLists[] = {commandList};
      commandQueue->ExecuteCommandLists(1, commandLists);
      // GPUとOSに画面の交換を行わせる
      swapChain->Present(1, 0);
      // 次のフレーム用のコマンドリストを準備
      hr = commandAllocator->Reset();
      // コマンドアロケータのリセットに失敗した場合はエラー
      assert(SUCCEEDED(hr));
      hr = commandList->Reset(commandAllocator, nullptr);
      // コマンドリストのリセットに失敗した場合はエラー
      assert(SUCCEEDED(hr));
    }
  }

  // 出力ウィンドウへの文字出力
  OutputDebugStringA("Hello,DirectX!\n");
  return 0;
}
