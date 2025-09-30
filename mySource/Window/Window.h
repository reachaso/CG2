#pragma once
#include "../DX12/DX12.h"
#include "../Log/Log.h"
#include <Windows.h>
#include <cstdint>
#include <string>

class Window {

public:
  // デストラクタで背景ブラシ解放
  ~Window();

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

  void Initialize(const char *windowTitle = "No Title",
                  const int32_t kClientWidth = 1280,
                  const int32_t kClientHeight = 720);

  // クリア色の設定＆クリア実行
  void SetClearColor(float r, float g, float b, float a = 1.0f);

  // クリア色の取得
  void
  ClearCurrentRT(ID3D12GraphicsCommandList *cmdList,
                 D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                 const D3D12_CPU_DESCRIPTOR_HANDLE *dsvOpt = nullptr) const;

  RECT wrc;
  HWND hwnd;

  // ==============================
  // dx12の初期化
  // ==============================
  DX12 dx12;

  // ==============================
  // ログの初期化
  // ==============================
  Log log;

private:
  WNDCLASS wc{};

  // Window が保持するクリア色と背景ブラシ
  float clearColor_[4] = {0.1f, 0.25f, 0.5f,
                          1.0f}; // 既存の青っぽい色を初期値に
                                 // 指定した色でクリアする

  HBRUSH hbrBackground_ = nullptr;

  // GDI 背景ブラシの更新（Initialize/SetClearColor から呼ぶ）
  void UpdateBackgroundBrush();
};
