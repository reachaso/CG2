#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <cstdint>
#include <dinput.h>
#include <windows.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {

public:
  Input(HWND hwnd);
  ~Input();

  IDirectInput8 *directInput = nullptr;
  IDirectInputDevice8 *keyboard = nullptr;

  BYTE key[256] = {};    // 256キーの状態を保持
  BYTE preKey[256] = {}; // 前フレームのキー状態を保持
  void Update();         // 毎フレーム呼び出す関数

  bool IsKeyPressed(uint8_t keyCode) const; // 押し続けてる状態（押しっぱなし）
  bool IsKeyUp(uint8_t keyCode) const;      // 押されてない状態
  bool IsKeyTrigger(uint8_t keyCode) const; // 今フレームで押された瞬間
  bool IsKeyRelease(uint8_t keyCode) const; // 今フレームで離された瞬間
};
