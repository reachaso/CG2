#include "Input.h"
#include <cassert>

Input::Input(HWND hwnd) {

  HRESULT hr =
      DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_HEADER_VERSION,
                         IID_IDirectInput8, (void **)&directInput, nullptr);
  assert(SUCCEEDED(hr));

  // キーボードデバイスの生成
  hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
  assert(SUCCEEDED(hr));
  // 入力データ形式の設定
  hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(hr));
  // 排他制御レベルの設定
  hr = keyboard->SetCooperativeLevel(
      hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
}

Input::~Input() {
  if (keyboard) {
    keyboard->Unacquire();
    keyboard->Release();
    keyboard = nullptr;
  }
  if (directInput) {
    directInput->Release();
    directInput = nullptr;
  }
}

void Input::Update() {
  memcpy(preKey, key, sizeof(key));
  keyboard->Acquire();
  keyboard->GetDeviceState(sizeof(key), &key);
}

bool Input::IsKeyPressed(uint8_t keyCode) const {
  return keyCode < 256 && (key[keyCode] & 0x80);
}

bool Input::IsKeyUp(uint8_t keyCode) const {
  return keyCode < 256 && !(key[keyCode] & 0x80);
}

bool Input::IsKeyTrigger(uint8_t keyCode) const {
  return keyCode < 256 && (key[keyCode] & 0x80) && !(preKey[keyCode] & 0x80);
}

bool Input::IsKeyRelease(uint8_t keyCode) const {
  return keyCode < 256 && !(key[keyCode] & 0x80) && (preKey[keyCode] & 0x80);
}
