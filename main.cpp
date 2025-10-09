#pragma warning(push)
#pragma warning(disable : 28251)
#include <Windows.h>
#pragma warning(pop)

#include "App.h"
#include <objbase.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  // COM 初期化
  HRESULT hrCoInt = CoInitializeEx(0, COINIT_MULTITHREADED);
  (void)hrCoInt; // 必要なら戻り値チェック

  App app;
  if (!app.Init()) {
    CoUninitialize();
    return -1;
  }

  const int code = app.Run();

  app.Term();
  CoUninitialize();
  return code;
}
