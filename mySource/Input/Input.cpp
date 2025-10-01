#include "Input.h"
#include <cassert>
#include "../../externals/imgui/imgui.h" 
#include <string>

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
  if (xinputState.Gamepad.wButtons != 0) {
    ZeroMemory(&xinputState, sizeof(XINPUT_STATE));
  }
}

void Input::Update() {
  memcpy(preKey, key, sizeof(key));
  keyboard->Acquire();
  keyboard->GetDeviceState(sizeof(key), &key);

  preMouseState = mouseState;
  mouse->Acquire();
  mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);

  preXinputState = xinputState;
  ZeroMemory(&xinputState, sizeof(XINPUT_STATE));
  XInputGetState(0, &xinputState);
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

void Input::SetXInputVibration(WORD leftMotor, WORD rightMotor) {
  XINPUT_VIBRATION vibration = {};
  vibration.wLeftMotorSpeed = leftMotor;   // 0～65535
  vibration.wRightMotorSpeed = rightMotor; // 0～65535
  XInputSetState(0, &vibration);
}

void Input::ControllerImGui(const char *label) {
  // ラベルをIDに使えるように##で衝突回避
  std::string base = label ? label : "コントローラー";
  if (!ImGui::CollapsingHeader((base + "##ControllerHeader").c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  // 接続状態
  ImGui::Text("state : ");
  ImGui::SameLine();

  const bool connected = IsXInputConnected();
  if (connected) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // 緑
    ImGui::Text("コントローラー接続中");
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255)); // 赤
    ImGui::Text("コントローラー未接続");
    ImGui::PopStyleColor();
    // 未接続ならここで終わり（UIはこれ以上いじれない）
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    return;
  }

  ImGui::SeparatorText("ボタン");

  auto onoff = [&](bool v) { return v ? "入力" : "未入力"; };

  ImGui::Text("A: %s", onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_A)));
  ImGui::Text("B: %s", onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_B)));
  ImGui::Text("X: %s", onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_X)));
  ImGui::Text("Y: %s", onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_Y)));
  ImGui::Text("LB: %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_LEFT_SHOULDER)));
  ImGui::Text("RB: %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)));
  ImGui::Text("START: %s", onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_START)));
  ImGui::Text("BACK : %s", onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_BACK)));
  ImGui::Text("Lスティック押し込み: %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_LEFT_THUMB)));
  ImGui::Text("Rスティック押し込み: %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_RIGHT_THUMB)));

  ImGui::SeparatorText("十字キー");
  ImGui::Text("十字上  : %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_UP)));
  ImGui::Text("十字下  : %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN)));
  ImGui::Text("十字左  : %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_LEFT)));
  ImGui::Text("十字右  : %s",
              onoff(IsXInputButtonPressed(XINPUT_GAMEPAD_DPAD_RIGHT)));

  ImGui::SeparatorText("トリガー");
  ImGui::Text("LT: %d", GetXInputLeftTrigger());
  ImGui::Text("RT: %d", GetXInputRightTrigger());

  ImGui::SeparatorText("スティック（生値）");
  ImGui::Text("左スティック: X=%d, Y=%d", GetXInputThumbLX(),
              GetXInputThumbLY());
  ImGui::Text("右スティック: X=%d, Y=%d", GetXInputThumbRX(),
              GetXInputThumbRY());

  ImGui::SeparatorText("振動");
  ImGui::SliderInt(("左モーター振動##" + base).c_str(), &leftVibration, 0,
                   65535);
  ImGui::SliderInt(("右モーター振動##" + base).c_str(), &rightVibration, 0,
                   65535);

  if (ImGui::Button(("振動開始##" + base).c_str())) {
    SetXInputVibration(static_cast<WORD>(leftVibration),
                       static_cast<WORD>(rightVibration));
  }
  ImGui::SameLine();
  if (ImGui::Button(("振動停止##" + base).c_str())) {
    SetXInputVibration(0, 0);
  }

  ImGui::Dummy(ImVec2(0.0f, 5.0f));
}
