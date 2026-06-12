#pragma once
#include "EngineConfig.h"
#include <memory>
#include "InputCommon.h"

// 各デバイスクラスの前方宣言
class Keyboard;
class Mouse;
class Controller;

/// @class Input
/// @brief 全ての入力デバイス（キーボード、マウス、コントローラー）を統合管理するクラス
/// @details DirectInput と XInput をラップし、各デバイスの状態更新とアクセスを提供します。
/// 従来のフラットな API との互換性維持のためのメソッドも提供しています。
class Input {
public:
    /// @brief 初期化
    /// @param hwnd ウィンドウハンドル
    Input(HWND hwnd);
    ~Input();

    /// @brief 全てのデバイスの状態を更新する（毎フレーム呼び出す必要があります）
    void Update();

    /// @brief Viewportがホバーされているか設定する
    void SetViewportHovered(bool hovered) { viewportHovered_ = hovered; }

    /// @brief Viewportがホバーされているか取得する
    bool IsViewportHovered() const { return viewportHovered_; }

    // デバイスへのアクセス

    /// @brief キーボード管理オブジェクトを取得する
    /// @return Keyboard ポインタ
    Keyboard* GetKeyboard() const { return keyboard_.get(); }

    /// @brief マウス管理オブジェクトを取得する
    /// @return Mouse ポインタ
    Mouse* GetMouse() const { return mouse_.get(); }

    /// @brief コントローラー管理オブジェクトを取得する
    /// @return Controller ポインタ
    Controller* GetController() const { return controller_.get(); }

    // ============================
    // キーボード入力 (互換性維持用)
    // ============================

    /// @brief 指定したキーが押されているか判定する
    /// @param keyCode DIK_* 定数
    /// @return 押されていれば true
    bool IsKeyPressed(uint8_t keyCode) const;

    /// @brief 指定したキーが押されていないか判定する
    /// @param keyCode DIK_* 定数
    /// @return 押されていなければ true
    bool IsKeyUp(uint8_t keyCode) const;

    /// @brief 指定したキーが押された瞬間か判定する
    /// @param keyCode DIK_* 定数
    /// @return 押された瞬間なら true
    bool IsKeyTrigger(uint8_t keyCode) const;

    /// @brief 指定したキーが離された瞬間か判定する
    /// @param keyCode DIK_* 定数
    /// @return 離された瞬間なら true
    bool IsKeyRelease(uint8_t keyCode) const;

    // ============================
    // マウス入力 (互換性維持用)
    // ============================

    /// @brief マウスの X 軸移動量を取得する
    /// @return 移動量
    LONG GetMouseX() const;

    /// @brief マウスの Y 軸移動量を取得する
    /// @return 移動量
    LONG GetMouseY() const;

    /// @brief マウスホイールの回転量を取得する
    /// @return 回転量
    LONG GetMouseZ() const;

    /// @brief マウスボタンが押されているか判定する
    /// @param button ボタン番号 (0:左, 1:右, 2:中)
    /// @return 押されていれば true
    bool IsMousePressed(int button) const;

    /// @brief マウスボタンが押された瞬間か判定する
    /// @param button ボタン番号
    /// @return 押された瞬間なら true
    bool IsMouseTrigger(int button) const;

    /// @brief マウスボタンが離された瞬間か判定する
    /// @param button ボタン番号
    /// @return 離された瞬間なら true
    bool IsMouseRelease(int button) const;

    // ============================
    // コントローラー入力 (互換性維持用)
    // ============================

    /// @brief コントローラーが接続されているか確認する
    /// @return 接続されていれば true
    bool IsXInputConnected() const;

    /// @brief コントローラーボタンが押されているか判定する
    /// @param button XINPUT_GAMEPAD_* 定数
    /// @return 押されていれば true
    bool IsXInputButtonPressed(WORD button) const;

    /// @brief コントローラーボタンが押された瞬間か判定する
    /// @param button XINPUT_GAMEPAD_* 定数
    /// @return 押された瞬間なら true
    bool IsXInputButtonTrigger(WORD button) const;

    /// @brief コントローラーボタンが離された瞬間か判定する
    /// @param button XINPUT_GAMEPAD_* 定数
    /// @return 離された瞬間なら true
    bool IsXInputButtonRelease(WORD button) const;

    /// @brief 左スティックの X 軸入力を取得する
    /// @return -32768 ～ 32767 の値
    SHORT GetXInputThumbLX() const;

    /// @brief 左スティックの Y 軸入力を取得する
    /// @return -32768 ～ 32767 の値
    SHORT GetXInputThumbLY() const;

    /// @brief 右スティックの X 軸入力を取得する
    /// @return -32768 ～ 32767 の値
    SHORT GetXInputThumbRX() const;

    /// @brief 右スティックの Y 軸入力を取得する
    /// @return -32768 ～ 32767 の値
    SHORT GetXInputThumbRY() const;

    /// @brief 左トリガーの押し込み量を取得する
    /// @return 0 ～ 255 の値
    BYTE GetXInputLeftTrigger() const;

    /// @brief 右トリガーの押し込み量を取得する
    /// @return 0 ～ 255 の値
    BYTE GetXInputRightTrigger() const;

    /// @brief コントローラーの振動を設定する
    /// @param leftMotor 左モーター強度 (0 ～ 65535)
    /// @param rightMotor 右モーター強度 (0 ～ 65535)
    void SetXInputVibration(WORD leftMotor, WORD rightMotor);

    /// @brief コントローラーのデバッグ UI を表示する
    /// @param label ウィンドウラベル
    void ControllerImGui(const char* label = "コントローラー");

    /// @brief Inputインスタンスを取得する（シングルトンアクセス用）
    /// @return Input ポインタ
    static Input* GetInstance() { return instance_; }

private:
    static Input* instance_;                 ///< シングルトン用インスタンスポインタ
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_; ///< DirectInput インターフェース

    std::unique_ptr<Keyboard> keyboard_;     ///< キーボード
    std::unique_ptr<Mouse> mouse_;           ///< マウス
    std::unique_ptr<Controller> controller_; ///< コントローラー

    bool viewportHovered_ = false;           ///< Viewportウィンドウがホバーされているか
};
