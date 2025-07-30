// DebugCamera.cpp
#include "DebugCamera.h"
#include "Affine/Affine.h" // MakePerspectiveFovMatrix, MakeAffineMatrix, Inverse
#include "Log/Log.h"       // Log::Debug
#include <cmath>           // sinf, cosf
#include <dinput.h>        // DIK_W など

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

  // ── キー入力による相対移動 ──
  if (input_->IsKeyPressed(DIK_W)) {
    translation_ = Add(translation_, Multiply(forward, moveSpeed_));
  }
  if (input_->IsKeyPressed(DIK_S)) {
    translation_ = Subtract(translation_, Multiply(forward, moveSpeed_));
  }
  if (input_->IsKeyPressed(DIK_A)) {
    translation_ = Subtract(translation_, Multiply(right, moveSpeed_));
  }
  if (input_->IsKeyPressed(DIK_D)) {
    translation_ = Add(translation_, Multiply(right, moveSpeed_));
  }
  if (input_->IsKeyPressed(DIK_Q)) {
    translation_.y -= moveSpeed_;
  }
  if (input_->IsKeyPressed(DIK_E)) {
    translation_.y += moveSpeed_;
  }

  // ── キー入力による回転 ──
  if (input_->IsKeyPressed(DIK_UP))
    rotation_.x -= rotateSpeed_;
  if (input_->IsKeyPressed(DIK_DOWN))
    rotation_.x += rotateSpeed_;
  if (input_->IsKeyPressed(DIK_LEFT))
    rotation_.y -= rotateSpeed_;
  if (input_->IsKeyPressed(DIK_RIGHT))
    rotation_.y += rotateSpeed_;

  // ── ビュー行列の再計算 ──
  Matrix4x4 world = MakeAffineMatrix({1, 1, 1}, // スケール固定
                                     rotation_, translation_);
  view_ = Inverse(world);
}

void DebugCamera::Reset() {
  translation_ = {0.0f, 0.0f, -5.0f}; // 原点にリセット
  rotation_ = {0.0f, 0.0f, 0.0f};    // 回転なしにリセット
}
