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
  hr = keyboard->SetDataFormat(&c_dfDIKeyboard); // 256 キー配列のフォーマット
  assert(SUCCEEDED(hr));
  hr = keyboard->SetCooperativeLevel(
      hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

  // マウスデバイスの生成
  hr = directInput->CreateDevice(GUID_SysMouse, &mouse, NULL);
  assert(SUCCEEDED(hr));
  hr = mouse->SetDataFormat(&c_dfDIMouse); // DIMOUSESTATE フォーマット
  assert(SUCCEEDED(hr));
  hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
}

// ------------------------------------------------------------
// デストラクタ
// ・念のためパッド振動を停止
// ・子（キーボード／マウス）→親（DirectInput）の順に Unacquire→Release
// ------------------------------------------------------------
Input::~Input() {

  SetXInputVibration(0, 0); // 事故防止：残留振動の停止

  if (mouse) {
    mouse->Unacquire(); // デバイスの所有権を解放
    mouse->Release();   // COM 参照カウントを減算
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

// ------------------------------------------------------------
// Update
// ・前フレーム状態を保存 → Acquire → GetDeviceState で最新状態を取得
// ・マウスも同様に差分値を取得。
// ・XInput は ZeroMemory→XInputGetState(0,...) で 0 番ポートをポーリング。
// ------------------------------------------------------------
void Input::Update() {
  // --- Keyboard ---
  memcpy(preKey, key, sizeof(key)); // 前フレーム退避
  keyboard->Acquire(); // 取得開始（フォーカス喪失後は失敗することがある）
  keyboard->GetDeviceState(sizeof(key), &key); // 256 バイトの生状態

  // --- Mouse ---
  preMouseState = mouseState; // 退避
  mouse->Acquire();
  mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState); // 相対移動&ボタン

  // --- XInput ---
  preXinputState = xinputState; // 退避
  ZeroMemory(&xinputState, sizeof(XINPUT_STATE));
  XInputGetState(0, &xinputState); // 0 番ポートの最新状態
}

// ============================
// キー判定ユーティリティ
// ・0x80 ビットが押下（スキャンコード形式の習慣）
// ・配列境界の防御として keyCode<256 をチェック
// ============================
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

// ============================
// マウス：ボタン判定と相対移動量
// ・DIMOUSESTATE::rgbButtons[n] の 0x80 で押下
// ・lX/lY は相対値（そのフレームで動いた量）。座標が欲しい場合は積算が必要。
// ============================
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

// 相対移動量・ホイール（チック）
LONG Input::GetMouseX() const { return mouseState.lX; }
LONG Input::GetMouseY() const { return mouseState.lY; }
LONG Input::GetMouseZ() const { return mouseState.lZ; }

// ============================
// XInput：接続確認とボタン／スティック／トリガ
// ・IsXInputConnected は即時問い合わせ（Update に依存しない）。
// ・ボタンのトリガ／リリースは wButtons のビット変化で判定。
// ============================
bool Input::IsXInputConnected() const {
  return XInputGetState(0, (XINPUT_STATE *)&xinputState) == ERROR_SUCCESS;
}
bool Input::IsXInputButtonPressed(WORD button) const {
  return (xinputState.Gamepad.wButtons & button) != 0;
}
bool Input::IsXInputButtonTrigger(WORD button) const {
  return (xinputState.Gamepad.wButtons & button) &&
         !(preXinputState.Gamepad.wButtons & button);
}
bool Input::IsXInputButtonRelease(WORD button) const {
  return !(xinputState.Gamepad.wButtons & button) &&
         (preXinputState.Gamepad.wButtons & button);
}
SHORT Input::GetXInputThumbLX() const { return xinputState.Gamepad.sThumbLX; }
SHORT Input::GetXInputThumbLY() const { return xinputState.Gamepad.sThumbLY; }
SHORT Input::GetXInputThumbRX() const { return xinputState.Gamepad.sThumbRX; }
SHORT Input::GetXInputThumbRY() const { return xinputState.Gamepad.sThumbRY; }
BYTE Input::GetXInputLeftTrigger() const {
  return xinputState.Gamepad.bLeftTrigger;
}
BYTE Input::GetXInputRightTrigger() const {
  return xinputState.Gamepad.bRightTrigger;
}

// ------------------------------------------------------------
// XInput 振動：左右モーターの強度を [0,65535] で指定
// ・ゲーム終了時やシーン遷移時は (0,0) を送って停止するのが安全。
// ------------------------------------------------------------
void Input::SetXInputVibration(WORD leftMotor, WORD rightMotor) {
  XINPUT_VIBRATION vibration = {};
  vibration.wLeftMotorSpeed = leftMotor;   // 0～65535
  vibration.wRightMotorSpeed = rightMotor; // 0～65535
  XInputSetState(0, &vibration);
}
