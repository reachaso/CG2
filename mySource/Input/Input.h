#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <cstdint>
#include <dinput.h>
#include <windows.h>
#include <Xinput.h>
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
  Input(HWND hwnd); // DirectInput 初期化 + KB/Mouse デバイス生成
  ~Input();         // 各デバイス解放＋（XInput 振動の念のための停止）

  // DirectInput 本体（親）。子デバイスより寿命は長く、解放は最後に。
  IDirectInput8 *directInput = nullptr;

  // 毎フレーム呼び出し：前フレームの状態を保存し最新状態を取得する
  void Update();

  // ============================
  // キーボード入力
  // ============================

  // キーボードデバイス（DirectInput）。Acquire/Unacquire を行う。
  IDirectInputDevice8 *keyboard = nullptr;

  // 256 キーの現在／前フレームの状態。各要素の 0x80 ビットが押下を示す。
  BYTE key[256] = {};
  BYTE preKey[256] = {};

  // 0x80 ビットで評価：押しっぱなし
  bool IsKeyPressed(uint8_t keyCode) const;
  // 非押下状態（押されていない）
  bool IsKeyUp(uint8_t keyCode) const;
  // 立ち上がり（今フレームで押された）
  bool IsKeyTrigger(uint8_t keyCode) const;
  // 立ち下がり（今フレームで離された）
  bool IsKeyRelease(uint8_t keyCode) const;

  // ============================
  // マウス入力
  // ============================

  // マウスデバイス（DirectInput）。相対移動とボタン状態を取得。
  IDirectInputDevice8 *mouse = nullptr;

  // 現在／前フレームのマウス状態。
  // ・lX/lY は相対移動量（ドライバ設定に依存）。
  // ・lZ はホイールの回転量（チック）。
  // ・rgbButtons[n] は 0x80 ビットで押下。
  DIMOUSESTATE mouseState = {};
  DIMOUSESTATE preMouseState = {};

  // 相対移動量／ホイール。UI で使う場合は積算や画面座標変換が必要。
  LONG GetMouseX() const;
  LONG GetMouseY() const;
  LONG GetMouseZ() const;

  // マウスボタンの押下判定（押しっぱ／トリガ／リリース）
  bool IsMousePressed(int button) const;
  bool IsMouseTrigger(int button) const;
  bool IsMouseRelease(int button) const;

  // ============================
  // コントローラー入力（XInput）
  // ============================

  // 今フレーム／前フレームの XInput 状態（ポート 0 を想定）
  XINPUT_STATE xinputState = {};
  XINPUT_STATE preXinputState = {};

  // 既定振動レベル（未使用のサンプル値）。SetXInputVibration
  // で直接指定するのが基本。
  int leftVibration = 30000;
  int rightVibration = 30000;

  // 接続確認（即時ポーリング）。ERROR_SUCCESS なら接続。
  bool IsXInputConnected() const;
  // 押しっぱなし／トリガ（立ち上がり）／リリース（立ち下がり）
  bool IsXInputButtonPressed(WORD button) const;
  bool IsXInputButtonTrigger(WORD button) const;
  bool IsXInputButtonRelease(WORD button) const;

  // スティック／トリガの生値（範囲：SHORT/BYTE）。デッドゾーン補正は呼び出し側で実施。
  SHORT GetXInputThumbLX() const;
  SHORT GetXInputThumbLY() const;
  SHORT GetXInputThumbRX() const;
  SHORT GetXInputThumbRY() const;
  BYTE GetXInputLeftTrigger() const;
  BYTE GetXInputRightTrigger() const;

  // --- XInput 振動制御：0～65535。終了時は 0,0 を送って停止するのがマナー。
  void SetXInputVibration(WORD leftMotor, WORD rightMotor);
};
