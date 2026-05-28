#pragma once
#include "DebugCamera/DebugCamera.h"
#include "Input/Input.h"
#include "MainCamera/MainCamera.h"
#include "Math/Math.h"

// Transform は別ヘッダで定義されている想定（ここではポインタだけ扱う）
struct Transform;

namespace RC {

/// @brief カメラのビュー行列と射影行列を保持する構造体
struct CameraMatrices {
  Matrix4x4 view; ///< ビュー行列
  Matrix4x4 proj; ///< 射影行列
};

/// @brief メインカメラとデバッグカメラを切り替えて管理するコントローラ
class CameraController {
public:
  /// @brief 初期化
  /// @param input 入力管理クラスへのポインタ
  /// @param mainPos メインカメラの初期座標
  /// @param mainRot メインカメラの初期回転
  /// @param fovY 垂直画角 (ラジアン)
  /// @param aspect アスペクト比 (width/height)
  /// @param nearZ ニアクリップ距離
  /// @param farZ ファークリップ距離
  void Initialize(Input *input, const Vector3 &mainPos, const Vector3 &mainRot,
                  float fovY, float aspect, float nearZ, float farZ);

  /// @brief 更新処理。TABキーによるカメラ切り替えや、各カメラの更新（追従など）を行う。
  /// @param dt 前フレームからの経過時間 (秒)。補間計算に使用。
  void Update(float dt = 1.0f / 60.0f);

  /// @brief 現在有効なカメラのビュー行列を取得する
  /// @return ビュー行列への参照
  const Matrix4x4 &GetView() const {
    return useDebug_ ? debug_.GetView() : main_.GetView();
  }

  /// @brief 現在有効なカメラの射影行列を取得する
  /// @return 射影行列への参照
  const Matrix4x4 &GetProjection() const {
    return useDebug_ ? debug_.GetProjection() : main_.GetProjection();
  }

  /// @brief 現在有効なカメラの行列セット（ビュー・射影）を取得する
  /// @return 行列セットを格納した構造体
  CameraMatrices GetMatrices() const {
    if (useDebug_) {
      return {debug_.GetView(), debug_.GetProjection()};
    } else {
      return {main_.GetView(), main_.GetProjection()};
    }
  }

  /// @brief ImGuiによるカメラ情報の描画とパラメータ調整
  void DrawImGui();

  /// @brief デバッグカメラの使用状態を設定する
  /// @param v trueでデバッグカメラを使用
  void SetUseDebug(bool v) { useDebug_ = v; }

  /// @brief デバッグカメラの使用中かどうかを取得する
  /// @return trueならデバッグカメラを使用中
  bool IsUsingDebug() const { return useDebug_; }

  /// @brief メインカメラの追従対象を設定する
  /// @param target 追従対象のTransformポインタ。nullptrで追従オフ。
  void SetTarget(const ::Transform *target);

  /// @brief 追従対象をクリア（オフ）する
  void ClearTarget() { SetTarget(nullptr); }

  /// @brief 追従対象が設定されているか取得する
  /// @return trueなら追従対象あり
  bool HasTarget() const { return target_ != nullptr; }

  /// @brief 追従時のカメラオフセットを設定する
  /// @param camOffset 対象に対する相対位置
  /// @param targetOffset 対象の注視点オフセット
  void SetFollowOffsets(const Vector3 &camOffset, const Vector3 &targetOffset);

  /// @brief カメラの追従強度（なめらかさ）を設定する
  void SetFollowSharpness(float camSharpness);

  /// @brief 先読み（LookAhead）に関するパラメータを設定する
  /// @param lookAhead 基本先読み距離
  /// @param lookAheadFactor 速度に応じた先読み倍率
  /// @param lookAheadSharpness 先読み移動のなめらかさ
  void SetLookAhead(float lookAhead, float lookAheadFactor,
                    float lookAheadSharpness);

  /// @brief 注視点追従のなめらかさを設定する
  void SetFocusSharpness(float focusSharpness);

  /// @brief Y軸（垂直方向）の追従特性を設定する
  /// @param deadZoneY 追従を抑える垂直範囲
  /// @param sharpnessUp 上昇時の追従強度
  /// @param sharpnessDown 下降時の追従強度
  /// @param maxSpeed 最大追従速度
  void SetFollowYSettings(float deadZoneY, float sharpnessUp,
                          float sharpnessDown, float maxSpeed);

  /// @brief 追従カメラの移動範囲制限を設定する
  /// @param left 左端
  /// @param right 右端
  /// @param bottom 下端
  /// @param top 上端
  /// @param enable 制限を有効にするかどうか
  void SetFollowBounds(float left, float right, float bottom, float top,
                       bool enable);

  /// @brief メインカメラの位置を直接設定する
  void SetMainPosition(const Vector3 &pos);

  /// @brief メインカメラの回転を直接設定する
  void SetMainRotation(const Vector3 &rot);

  /// @brief 現在のカメラのワールド座標を取得する
  /// @return ワールド座標
  RC::Vector3 GetWorldPos() const { return worldPos_; }

  /// @brief デバッグカメラの位置を直接設定する
  void SetDebugPosition(const Vector3 &pos);

  /// @brief デバッグカメラの回転を直接設定する
  void SetDebugRotation(const Vector3 &rot);

  /// @brief デバッグカメラの位置と回転を同時に設定する
  void SetDebugTransform(const Vector3 &pos, const Vector3 &rot);

  /// @brief 現在有効なカメラの位置を設定する
  void SetPosition(const Vector3 &pos);

  /// @brief 現在有効なカメラの回転を設定する
  void SetRotation(const Vector3 &rot);

  /// @brief 現在有効なカメラの位置と回転を設定する
  void SetTransform(const Vector3 &pos, const Vector3 &rot);

  /// @brief メインカメラの位置を取得する
  Vector3 GetMainPosition() const { return main_.GetPosition(); }

  /// @brief メインカメラの回転を取得する
  Vector3 GetMainRotation() const { return main_.GetRotation(); }

  /// @brief メインカメラの射影パラメータを実行時に変更する
  void SetProjection(float fovY, float aspect, float nearZ, float farZ) {
    main_.SetProjection(fovY, aspect, nearZ, farZ);
    fovY_ = fovY;
    aspect_ = aspect;
  }

  /// @brief 垂直画角を取得
  float GetFovY() const { return fovY_; }
  /// @brief ニアクリップ距離を取得
  float GetNearZ() const { return main_.GetNearZ(); }
  /// @brief ファークリップ距離を取得
  float GetFarZ() const { return main_.GetFarZ(); }
  /// @brief アスペクト比を取得
  float GetAspect() const { return aspect_; }

private:
  /// @brief 追従ロジックの内部更新
  void UpdateFollow_(float dt);

  Input *input_ = nullptr; ///< 入力管理クラスへのポインタ
  DebugCamera debug_;      ///< デバッグ用カメラ
  MainCamera main_;        ///< メイン（追従）カメラ
  bool useDebug_ = false;  ///< デバッグモードフラグ
  bool showGuide_ = false; ///< ガイドライン表示フラグ
  bool isDraggingCamera_ = false; ///< カメラのドラッグ操作中フラグ

  RC::Vector3 worldPos_{0, 0, 0}; ///< 現在のワールド座標

  const ::Transform *target_ = nullptr; ///< 追従対象のTransform

  /// @brief 追従範囲制限の設定
  struct FollowBounds2D {
    float left = 0.0f;    ///< 左端
    float right = 0.0f;   ///< 右端
    float bottom = 0.0f;  ///< 下端
    float top = 0.0f;     ///< 上端
    bool enabled = false; ///< 有効フラグ
  } bounds_;

  /// @brief 追従挙動のパラメータ
  struct FollowParams {
    Vector3 camOffset = {0.0f, 0.0f, -30.0f};  ///< カメラオフセット
    Vector3 targetOffset = {0.0f, 2.0f, 0.0f}; ///< 注視点オフセット
    float camSharpness = 15.0f;                ///< カメラ移動の追従強度
    float lookAhead = 2.0f;                    ///< 先読み距離
    float lookAheadFactor = 10.0f;             ///< 速度先読み係数
    float lookAheadSharpness = 6.0f;           ///< 先読み補間強度
    float focusSharpness = 10.0f;              ///< 注視点補間強度
    float deadZoneY = 1.2f;                    ///< Y軸デッドゾーン
    float ySharpnessUp = 2.0f;                 ///< Y上昇追従強度
    float ySharpnessDown = 12.0f;              ///< Y下降追従強度
    float yMaxSpeed = 6.0f;                    ///< Y最大速度
  } followParams_;

  /// @brief 追従計算の実行状態
  struct FollowState {
    Vector3 camPos = {0.0f, 0.0f, 0.0f};        ///< 補間計算後の座標
    Vector3 prevTargetPos = {0.0f, 0.0f, 0.0f}; ///< 前フレームの対象座標
    bool initialized = false;                   ///< 初期化フラグ
    Vector3 focus = {0.0f, 0.0f, 0.0f};         ///< 現在の注視点
    float lookAheadX = 0.0f;                    ///< 現在の先読み量
  } follow_;

  float fovY_ = 0.45f;          ///< 垂直画角
  float aspect_ = 16.0f / 9.0f; ///< アスペクト比
};

} // namespace RC
