#include "Window.h"
#include <cstring>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "../Common/EngineConfig.h"
#include "../Common/Log/Log.h"
#include <format>

#pragma comment(lib, "winmm.lib") // timeGetTimeを使う場合に必要

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd,
                                                             UINT msg,
                                                             WPARAM wparam,
                                                             LPARAM lparam);

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

  // Alt + 左クリックでボーダーレスウィンドウをドラッグ移動できるようにする
  if (msg == WM_NCHITTEST) {
    if (GetAsyncKeyState(VK_MENU) & 0x8000) {
      return HTCAPTION;
    }
  }

  #if RC_ENABLE_IMGUI
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true; // ImGuiが処理した場合はtrueを返す
  }
#endif

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
                        const int32_t kClientHeight, bool fullscreen) {

  timeBeginPeriod(1); // タイマー精度を1msに設定

  // ==============================
  // ウィンドウの初期化
  // ==============================
  std::wstring wTitle = windowTitle ? Utf8ToWString(windowTitle) : L"No Title";

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
  // ウィンドウスタイルを設定
#if defined(_DEBUG) || defined(RC_DEVELOPMENT)
  DWORD style = WS_POPUP | WS_VISIBLE;
#else
  DWORD style = fullscreen ? (WS_POPUP | WS_VISIBLE) : WS_OVERLAPPEDWINDOW;
#endif

  RECT wrc = {0, 0, kClientWidth, kClientHeight};
  AdjustWindowRect(&wrc, style, false);

  int windowW = wrc.right - wrc.left;
  int windowH = wrc.bottom - wrc.top;
  
  int x = 0;
  int y = 0;
  if (!fullscreen) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    x = (screenW - windowW) / 2;
    y = (screenH - windowH) / 2;
  }

  // ウィンドウを作成
  hwnd = CreateWindow(wc.lpszClassName,     // 利用するクラス名
                      wTitle.c_str(),       // ウィンドウのタイトル
                      style,                // ウィンドウスタイル
                      x,                    // x座標
                      y,                    // y座標
                      windowW,              // 幅
                      windowH,              // 高さ
                      nullptr,              // 親ウィンドウハンドル
                      nullptr,              // メニューハンドル
                      wc.hInstance,         // インスタンスハンドル
                      nullptr               // オプション
  );

  // ウィンドウを表示
  ShowWindow(hwnd, SW_SHOW);

  Log::Print(std::format("[Window] Created: \"{}\" ({}x{})", windowTitle, kClientWidth, kClientHeight));
}

void Window::Resize(int& outWidth, int& outHeight, bool fullscreen) {
  if (!hwnd) return;

#if defined(_DEBUG) || defined(RC_DEVELOPMENT)
  DWORD style = WS_POPUP | WS_VISIBLE;
#else
  DWORD style = fullscreen ? (WS_POPUP | WS_VISIBLE) : WS_OVERLAPPEDWINDOW;
#endif
  
  SetWindowLongPtr(hwnd, GWL_STYLE, style);

  if (fullscreen) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenW, screenH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    outWidth = screenW;
    outHeight = screenH;
  } else {
    RECT wrc = {0, 0, outWidth, outHeight};
    AdjustWindowRect(&wrc, style, false);
    
    // ウィンドウをプライマリモニタの中央に配置する
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int windowW = wrc.right - wrc.left;
    int windowH = wrc.bottom - wrc.top;
    int x = (screenW - windowW) / 2;
    int y = (screenH - windowH) / 2;
    
    SetWindowPos(hwnd, HWND_TOP, x, y, windowW, windowH, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }
  Log::Print(std::format("[Window] Resized: ({}x{}) Fullscreen:{}", outWidth, outHeight, fullscreen));
}

static std::wstring Utf8ToWStringImpl(const char *s) {
  if (!s)
    return L"";
  int lenW = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
  if (lenW <= 0)
    return L"";
  std::wstring w(lenW - 1, L'\0'); // 終端NULぶんは削る
  MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), lenW);
  return w;
}

// クラスの静的メンバとして
std::wstring Window::Utf8ToWString(const char *s) {
  return Utf8ToWStringImpl(s);
}

void Window::SetTitleUTF8(const char *utf8) {
  if (!hwnd)
    return;
  std::wstring w = Utf8ToWStringImpl(utf8);
  SetWindowTextW(hwnd, w.c_str());
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
