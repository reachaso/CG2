#pragma once
#include "IComponent.h"

/// @brief カメラの射影パラメータを保持するコンポーネント
/// @details Entity に TransformComponent と共にアタッチすることで、
/// そのEntityをカメラとして機能させる。
struct CameraComponent : public IComponent {
    float fovY = 0.45f;       ///< 垂直画角 (ラジアン)
    float nearZ = 0.1f;       ///< ニアクリップ距離
    float farZ = 100.0f;      ///< ファークリップ距離
    bool isMain = true;       ///< メインカメラフラグ（将来複数カメラ対応用）
};
