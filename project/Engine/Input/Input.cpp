#include "Input.h"
#include "Keyboard/Keyboard.h"
#include "Mouse/Mouse.h"
#include "Controller/Controller.h"
#include <cassert>

Input* Input::instance_ = nullptr;

Input::Input(HWND hwnd) {
    instance_ = this;
    HRESULT hr = DirectInput8Create(
        GetModuleHandle(nullptr), DIRECTINPUT_HEADER_VERSION,
        IID_IDirectInput8, (void**)directInput_.GetAddressOf(), nullptr);
    assert(SUCCEEDED(hr));

    // 各デバイスのインスタンスを生成
    keyboard_ = std::make_unique<Keyboard>(directInput_.Get(), hwnd);
    mouse_ = std::make_unique<Mouse>(directInput_.Get(), hwnd);
    controller_ = std::make_unique<Controller>();
}

Input::~Input() {
    // 各デバイスのデストラクタが呼ばれる
    instance_ = nullptr;
}

void Input::Update() {
    if (keyboard_) keyboard_->Update();
    if (mouse_) mouse_->Update();
    if (controller_) controller_->Update();
}

// ============================
// キーボード入力 (ラッパー)
// ============================
bool Input::IsKeyPressed(uint8_t keyCode) const { return keyboard_->IsPressed(keyCode); }
bool Input::IsKeyUp(uint8_t keyCode) const { return keyboard_->IsKeyUp(keyCode); }
bool Input::IsKeyTrigger(uint8_t keyCode) const { return keyboard_->IsTrigger(keyCode); }
bool Input::IsKeyRelease(uint8_t keyCode) const { return keyboard_->IsRelease(keyCode); }

// ============================
// マウス入力 (ラッパー)
// ============================
LONG Input::GetMouseX() const { return mouse_->GetX(); }
LONG Input::GetMouseY() const { return mouse_->GetY(); }
LONG Input::GetMouseZ() const { return mouse_->GetZ(); }
bool Input::IsMousePressed(int button) const { return mouse_->IsPressed(button); }
bool Input::IsMouseTrigger(int button) const { return mouse_->IsTrigger(button); }
bool Input::IsMouseRelease(int button) const { return mouse_->IsRelease(button); }

// ============================
// コントローラー入力 (ラッパー)
// ============================
bool Input::IsXInputConnected() const { return controller_->IsConnected(); }
bool Input::IsXInputButtonPressed(WORD button) const { return controller_->IsButtonPressed(button); }
bool Input::IsXInputButtonTrigger(WORD button) const { return controller_->IsButtonTrigger(button); }
bool Input::IsXInputButtonRelease(WORD button) const { return controller_->IsButtonRelease(button); }
SHORT Input::GetXInputThumbLX() const { return controller_->GetThumbLX(); }
SHORT Input::GetXInputThumbLY() const { return controller_->GetThumbLY(); }
SHORT Input::GetXInputThumbRX() const { return controller_->GetThumbRX(); }
SHORT Input::GetXInputThumbRY() const { return controller_->GetThumbRY(); }
BYTE Input::GetXInputLeftTrigger() const { return controller_->GetLeftTrigger(); }
BYTE Input::GetXInputRightTrigger() const { return controller_->GetRightTrigger(); }
void Input::SetXInputVibration(WORD leftMotor, WORD rightMotor) { controller_->SetVibration(leftMotor, rightMotor); }
void Input::ControllerImGui(const char* label) { controller_->DrawImGui(label); }
