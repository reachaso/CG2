#pragma once
#include "DebugCamera/DebugCamera.h"
#include "Input/Input.h"
#include "MainCamera/MainCamera.h"
#include "Math/Math.h"

struct CameraMatrices {
  Matrix4x4 view;
  Matrix4x4 proj;
};

class CameraController {
public:
  // 画角やクリップは共通、初期姿勢だけ可変にしておきます
  void Initialize(Input *input, const Vector3 &mainPos, const Vector3 &mainRot,
                  float fovY, float aspect, float nearZ, float farZ);

  // TAB 切替と各カメラの更新
  void Update();

  // いま有効なカメラの行列
  const Matrix4x4 &GetView() const {
    return useDebug_ ? debug_.GetView() : main_.GetView();
  }
  const Matrix4x4 &GetProjection() const {
    return useDebug_ ? debug_.GetProjection() : main_.GetProjection();
  }

  CameraMatrices GetMatrices() const {
    if (useDebug_) {
      return {debug_.GetView(), debug_.GetProjection()};
    } else {
      return {main_.GetView(), main_.GetProjection()};
    }
  }

  void DrawImGui();

  // 外側から（例えばボタンで）モードを明示切替したい場合用
  void SetUseDebug(bool v) { useDebug_ = v; }
  bool IsUsingDebug() const { return useDebug_; }

private:
  Input *input_ = nullptr;
  DebugCamera debug_;
  MainCamera main_;
  bool useDebug_ = false;
  bool showGuide_ = false;
};
