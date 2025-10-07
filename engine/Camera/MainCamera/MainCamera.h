#pragma once
#include "Math/Math.h"
#include "struct.h"

class MainCamera {
public:
  /// 初期化
  ///   pos    : カメラのワールド座標
  ///   rot    : カメラの回転 (ラジアン)
  ///   fovY   : 垂直画角 (ラジアン)
  ///   aspect : 画面アスペクト比 (width/height)
  ///   nearZ, farZ: クリップ距離
  void Initialize(const Vector3 &pos, const Vector3 &rot, float fovY,
                  float aspect, float nearZ, float farZ);

  /// 毎フレーム呼ぶ（位置や向きを動的に変えたいならここで）
  void Update();

  /// シェーダに渡す行列
  const Matrix4x4 &GetView() const { return view_; }
  const Matrix4x4 &GetProjection() const { return proj_; }

  /// 外部から位置や回転を変更したいとき
  void SetPosition(const Vector3 &pos) { transform_.translation = pos; }
  void SetRotation(const Vector3 &rot) { transform_.rotation = rot; }

private:
  Transform transform_; // scale, rotation, translation
                        // :contentReference[oaicite:1]{index=1}
  Matrix4x4 view_;
  Matrix4x4 proj_;
};
