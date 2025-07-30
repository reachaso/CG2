#include "Window.h"

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

void Window::Initialize(const char *windowTitle, const int32_t kClientWidth,
                        const int32_t kClientHeight) {
   
  dx12.Initialize();
  log.Initialize();

  // ==============================
  // ウィンドウの初期化
  // ==============================
  std::wstring wTitle;
  if (windowTitle) {
    wTitle = std::wstring(windowTitle, windowTitle + strlen(windowTitle));
  } else {
    wTitle = L"CG2";
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
