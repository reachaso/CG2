#include "App.h"
#include "AppConfig.h"
#include <Windows.h>
#include <objbase.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  HRESULT hrCo = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hrCo)) {
    return -1;
  }

  AppConfig appCfg; // 既存のデフォルト値を使用
  App app(appCfg);
  int ret = app.Run();

  CoUninitialize();
  return ret;
}
