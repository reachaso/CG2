#pragma once
#include "Log/Log.h"
#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <string>

class Window {

public:
  static const int32_t kDefaultWidth = 1280;
  static const int32_t kDefaultHeight = 720;

  // デストラクタで背景ブラシ解放
  ~Window();

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

  void Initialize(const char *windowTitle = "No Title",
                  const int32_t kClientWidth = kDefaultWidth,
                  const int32_t kClientHeight = kDefaultHeight);

  // クリア色の設定＆クリア実行
  void SetClearColor(float r, float g, float b, float a = 1.0f);

  // クリア色の取得
  void
  ClearCurrentRT(ID3D12GraphicsCommandList *cmdList,
                 D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                 const D3D12_CPU_DESCRIPTOR_HANDLE *dsvOpt = nullptr) const;

  HWND GetHwnd() const { return hwnd; }
  RECT GetClientRect() const { return wrc; }
  HINSTANCE GetInstance() const { return wc.hInstance; }

  // ==============================
  // ログの初期化
  // ==============================
  Log log;

private:
  WNDCLASS wc{};

  // Window が保持するクリア色と背景ブラシ
  float clearColor_[4] = {0.1f, 0.25f, 0.5f, 1.0f}; // 指定した色でクリアする

  HBRUSH hbrBackground_ = nullptr;

  // GDI 背景ブラシの更新（Initialize/SetClearColor から呼ぶ）
  void UpdateBackgroundBrush();
  
  RECT wrc;
  HWND hwnd;
};
