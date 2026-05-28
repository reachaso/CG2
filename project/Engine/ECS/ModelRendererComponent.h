#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"

/// @brief 3Dモデルの描画を担当するコンポーネント
/// モデルのハンドルを保持し、RenderSystem による自動描画の対象となります。
class ModelRendererComponent : public IComponent {
public:
  /// @brief モデルハンドル
  /// RC::LoadModel() によって返されたIDを保持します。
  int modelHandle = -1;

  /// @brief テクスチャのオーバーライド
  /// マテリアル設定とは別のテクスチャを強制する場合に使用します（-1 でデフォルト）。
  int texOverride = -1;

  /// @brief 描画の可視性フラグ
  bool visible = true;

  /// @brief ブレンドモード
  /// 描画時のブレンディング設定を指定します。
  int blendMode = 0;

  /// @brief モデルの色（乗算カラー）
  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};

  /// @brief 環境マップ映り込み係数（0=映り込みなし、1=完全鏡面）
  float environmentCoeff = 0.0f;

  /// @brief 有効なモデルが設定されているか確認
  /// @return 有効なハンドルを保持していれば true
  bool HasModel() const { return modelHandle >= 0; }
};
