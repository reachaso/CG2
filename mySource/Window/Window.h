#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include "DX12/DX12.h"
#include "Log/Log.h"
#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_dx12.h"
#include "../externals/imgui/imgui_impl_win32.h"

class Window {

public:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

  void Initialize(const char *windowTitle = "CG2",
                  const int32_t kClientWidth = 1280,
                  const int32_t kClientHeight = 720);

  RECT wrc = {};
  HWND hwnd = nullptr;

  // ==============================
  // dx12の初期化
  // ==============================
  //DX12 dx12;

  // ==============================
  // ログの初期化
  // ==============================

  Log log;

private:
  WNDCLASS wc{};
};
