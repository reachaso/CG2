// DebugCamera.cpp
#include "DebugCamera.h"
#include "Math/Math.h" // MakePerspectiveFovMatrix, MakeAffineMatrix, Inverse
#include "Log/Log.h"       // Log::Debug
#include <cmath>           // sinf, cosf
#include <dinput.h>        // DIK_W など

using namespace RC;

void DebugCamera::Initialize(Input *input, float fovY, float aspect,
                             float nearZ, float farZ) {
  input_ = input;
  // 射影行列を一度だけ作成
  proj_ = MakePerspectiveFovMatrix(fovY, aspect, nearZ, farZ);
}

void DebugCamera::Update() {

  // ── カメラの向きから Forward／Right を計算 ──
  float yaw = rotation_.y;   // Yaw（Y軸回転）
  float pitch = rotation_.x; // Pitch（X軸回転）
  Vector3 forward = {std::sinf(yaw) * std::cosf(pitch), std::sinf(pitch),
                     std::cosf(yaw) * std::cosf(pitch)};
  Vector3 right = {std::cosf(yaw), 0.0f, -std::sinf(yaw)};
  Vector3 up = {0.0f, 1.0f, 0.0f};

  // ── キー入力による相対移動 ──
  if (input_->IsKeyPressed(DIK_W)) {
    translation_ =
        Add(translation_, Multiply(forward, moveSpeed_ * deltaTime_));
  }
  if (input_->IsKeyPressed(DIK_S)) {
    translation_ =
        Subtract(translation_, Multiply(forward, moveSpeed_ * deltaTime_));
  }
  if (input_->IsKeyPressed(DIK_A)) {
    translation_ =
        Subtract(translation_, Multiply(right, moveSpeed_ * deltaTime_));
  }
  if (input_->IsKeyPressed(DIK_D)) {
    translation_ = Add(translation_, Multiply(right, moveSpeed_ * deltaTime_));
  }
  if (input_->IsKeyPressed(DIK_Q)) {
    translation_.y += moveSpeed_ * deltaTime_;
  }
  if (input_->IsKeyPressed(DIK_E)) {
    translation_.y -= moveSpeed_ * deltaTime_;
  }

  // ── マウスホイールによる前後移動 ──
  LONG wheel = input_->GetMouseZ();
  if (wheel != 0) {
    float wheelSpeed = 0.01f; // ホイール感度
    translation_ = Add(translation_, Multiply(forward, wheel * wheelSpeed));
  }

  // ── 右クリックドラッグによる回転 ──
  if (input_->IsMousePressed(1)) { // 1: 右クリック
    float mouseRotateSpeed = 0.1f * deltaTime_; // マウス感度
    rotation_.y += input_->GetMouseX() * mouseRotateSpeed;
    rotation_.x += input_->GetMouseY() * mouseRotateSpeed;
  }

  // ── 中央ボタンドラッグによる平行移動 ──
  if (input_->IsMousePressed(2)) {            // 2: 中央ボタン
    float mouseMoveSpeed = 0.3f * deltaTime_; // 平行移動感度
    // Xはright方向、Yはup方向
    translation_ = Add(translation_,
                       Multiply(right, input_->GetMouseX() * mouseMoveSpeed * -1));
    translation_ =
        Add(translation_, Multiply(up, -input_->GetMouseY() * mouseMoveSpeed * -1));
  }

  // ── キー入力による回転 ──
  if (input_->IsKeyPressed(DIK_UP))
    rotation_.x -= rotateSpeed_ * deltaTime_;
  if (input_->IsKeyPressed(DIK_DOWN))
    rotation_.x += rotateSpeed_ * deltaTime_;
  if (input_->IsKeyPressed(DIK_LEFT))
    rotation_.y -= rotateSpeed_ * deltaTime_;
  if (input_->IsKeyPressed(DIK_RIGHT))
    rotation_.y += rotateSpeed_ * deltaTime_;

  // ── ビュー行列の再計算 ──
  Matrix4x4 world = MakeAffineMatrix({1, 1, 1}, // スケール固定
                                     rotation_, translation_);
  view_ = Inverse(world);
}

void DebugCamera::Reset() {
  translation_ = {0.0f, 0.0f, -8.0f}; // 原点にリセット
  rotation_ = {0.0f, 0.0f, 0.0f};     // 回転なしにリセット
}

void DebugCamera::SetPosition(const Vector3 &pos) {
  translation_ = pos;
  RebuildView_();
}

void DebugCamera::SetRotation(const Vector3 &rot) {
  rotation_ = rot;
  RebuildView_();
}

void DebugCamera::SetTransform(const Vector3 &pos, const Vector3 &rot) {
  translation_ = pos;
  rotation_ = rot;
  RebuildView_();
}

// ==================
// view再計算
// ==================
void DebugCamera::RebuildView_() {
  Matrix4x4 world = MakeAffineMatrix({1, 1, 1}, rotation_, translation_);
  view_ = Inverse(world);
}
