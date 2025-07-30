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
  hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(hr));
  hr = keyboard->SetCooperativeLevel(
      hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

  // マウスデバイスの生成
  hr = directInput->CreateDevice(GUID_SysMouse, &mouse, NULL);
  assert(SUCCEEDED(hr));
  hr = mouse->SetDataFormat(&c_dfDIMouse);
  assert(SUCCEEDED(hr));
  hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
}

Input::~Input() {
  if (mouse) {
    mouse->Unacquire();
    mouse->Release();
    mouse = nullptr;
  }
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

  preMouseState = mouseState;
  mouse->Acquire();
  mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
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


// マウスボタン
bool Input::IsMousePressed(int button) const {
  return (mouseState.rgbButtons[button] & 0x80);
}
bool Input::IsMouseTrigger(int button) const {
  return (mouseState.rgbButtons[button] & 0x80) &&
         !(preMouseState.rgbButtons[button] & 0x80);
}
bool Input::IsMouseRelease(int button) const {
  return !(mouseState.rgbButtons[button] & 0x80) &&
         (preMouseState.rgbButtons[button] & 0x80);
}

// マウス座標・ホイール
LONG Input::GetMouseX() const { return mouseState.lX; }
LONG Input::GetMouseY() const { return mouseState.lY; }
LONG Input::GetMouseZ() const { return mouseState.lZ; }
