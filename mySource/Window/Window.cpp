#include "Window.h"

namespace {
// ウィンドウクラス名。プロセスごとに一意になるようGUIDを付加してもOK
const wchar_t *kWndClassName = L"CG2WindowClass";
} // namespace

Window::Window(HINSTANCE hInst)
    : hInst_(hInst), hWnd_(nullptr), quit_(false), width_(0), height_(0),
      className_(kWndClassName), customHandler_(nullptr) {}

Window::~Window() {
  if (hWnd_) {
    DestroyWindow(hWnd_);
    hWnd_ = nullptr;
  }
  UnregisterClass(className_.c_str(), hInst_);
}

bool Window::Create(const std::wstring &title, int w, int h) {
  width_ = w;
  height_ = h;

  // 1. ウィンドウクラス登録
  WNDCLASS wc = {};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = StaticWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = sizeof(void *);
  wc.hInstance = hInst_;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = className_.c_str();
  if (!RegisterClass(&wc))
    return false;

  // 2. ウィンドウサイズ調整
  RECT wr = {0, 0, width_, height_};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

  // 3. ウィンドウ生成
  hWnd_ = CreateWindowW(className_.c_str(), title.c_str(), WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left,
                        wr.bottom - wr.top, nullptr, nullptr, hInst_,
                        this // WM_NCCREATE で this を受け取るため
  );
  if (!hWnd_)
    return false;

  ShowWindow(hWnd_, SW_SHOW);
  UpdateWindow(hWnd_);
  return true;
}

void Window::PollMessages() {
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      quit_ = true;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

LRESULT CALLBACK Window::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam) {
  Window *win = nullptr;
  if (msg == WM_NCCREATE) {
    // Create() 呼び出し時に渡した this ポインタを保存
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
    win = reinterpret_cast<Window *>(cs->lpCreateParams);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(win));
  } else {
    win = reinterpret_cast<Window *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  }

  if (win) {
    return win->WndProc(hWnd, msg, wParam, lParam);
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // ImGui など外部ライブラリ向けのハンドラを先に呼びたい場合はここで分岐

  // カスタム登録されたハンドラがあれば優先的に処理
  if (customHandler_) {
    LRESULT result = customHandler_(hWnd, msg, wParam, lParam);
    if (result != 0)
      return result;
  }

  switch (msg) {
  case WM_SIZE:
    width_ = LOWORD(lParam);
    height_ = HIWORD(lParam);
    // レンダーターゲットの再作成フラグを立てるなど
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  // キー入力／マウス入力などもここで拾える
  case WM_KEYDOWN:
    // wParam に仮想キーコード
    return 0;

  default:
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }
}
