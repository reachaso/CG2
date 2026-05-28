#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"

/// @brief スカイボックスの描画を担当するコンポーネント
/// RC::CreateSkyBox() で生成されたハンドルを保持します。
class SkyboxComponent : public IComponent {
public:
  /// @brief スカイボックスハンドル
  int skyboxHandle = -1;

  /// @brief 描画の可視性フラグ
  bool visible = true;

  /// @brief 色（乗算カラー）
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  /// @brief 有効なハンドルが設定されているか確認
  /// @return 有効なハンドルを保持していれば true
  bool HasSkybox() const { return skyboxHandle >= 0; }
};
