#include "Window.h"
#include <cstring>

// 背景ブラシ解放
Window::~Window() {
  if (hbrBackground_) {
    DeleteObject(hbrBackground_);
    hbrBackground_ = nullptr;
  }
}

// ウィンドウプロシージャ
LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

// 背景ブラシ更新（clearColor_ を GDI COLORREF に変換）
void Window::UpdateBackgroundBrush() {
  if (hbrBackground_) {
    DeleteObject(hbrBackground_);
    hbrBackground_ = nullptr;
  }
  auto toByte = [](float v) -> int {
    if (v < 0.0f)
      v = 0.0f;
    if (v > 1.0f)
      v = 1.0f;
    return static_cast<int>(v * 255.0f + 0.5f);
  };
  COLORREF c = RGB(toByte(clearColor_[0]), toByte(clearColor_[1]),
                   toByte(clearColor_[2]));
  hbrBackground_ = CreateSolidBrush(c);
  wc.hbrBackground = hbrBackground_;
}

void Window::Initialize(const char *windowTitle, const int32_t kClientWidth,
                        const int32_t kClientHeight) {

  dx12.Initialize();
  log.Initialize();

  // ==============================
  // ウィンドウの初期化
  // ==============================
  std::wstring wTitle;
  if (windowTitle) {
    wTitle = std::wstring(windowTitle, windowTitle + std::strlen(windowTitle));
  } else {
    wTitle = L"No Title";
  }

  // ==============================
  // ウィンドウクラスの設定
  // ==============================

  // ウィンドウプロシージャを設定
  wc.lpfnWndProc = WindowProc;
  // ウィンドウクラスの名前を設定
  wc.lpszClassName = L"SampleWindowClass";
  // インスタンスハンドル
  wc.hInstance = GetModuleHandle(nullptr);
  // カーソル
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  // ウィンドウクラスを登録
  RegisterClass(&wc);

  wrc = {0, 0, kClientWidth, kClientHeight};
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

  // ウィンドウを作成
  hwnd = CreateWindow(wc.lpszClassName,     // 利用するクラス名
                      wTitle.c_str(),       // ウィンドウのタイトル
                      WS_OVERLAPPEDWINDOW,  // ウィンドウスタイル
                      CW_USEDEFAULT,        // x座標
                      CW_USEDEFAULT,        // y座標
                      wrc.right - wrc.left, // 幅
                      wrc.bottom - wrc.top, // 高さ
                      nullptr,              // 親ウィンドウハンドル
                      nullptr,              // メニューハンドル
                      wc.hInstance,         // インスタンスハンドル
                      nullptr               // オプション
  );

  // ウィンドウを表示
  ShowWindow(hwnd, SW_SHOW);
}

// クリア色の更新（GDI 背景も同期）
void Window::SetClearColor(float r, float g, float b, float a) {
  clearColor_[0] = r;
  clearColor_[1] = g;
  clearColor_[2] = b;
  clearColor_[3] = a;
  UpdateBackgroundBrush();
}

// RT/DS のクリアを Window 経由で実行
void Window::ClearCurrentRT(ID3D12GraphicsCommandList *cmdList,
                            D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                            const D3D12_CPU_DESCRIPTOR_HANDLE *dsvOpt) const {
  cmdList->ClearRenderTargetView(rtv, clearColor_, 0, nullptr);
  if (dsvOpt) {
    cmdList->ClearDepthStencilView(*dsvOpt, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0,
                                   nullptr);
  }
}
