#pragma once
#include "Math/MathTypes.h"
#include <d3d12.h>
#include <vector>

namespace RC {

struct WaveSource {
  RC::Vector2 uv;
  float radius;
  float strength;
};

/// @brief インタラクティブな波紋シミュレーションを初期化します
void InitInteractiveWater();

/// @brief シミュレーションリソースを破棄します
void TermInteractiveWater();

/// @brief 波源を追加します（毎フレームの Update 呼び出し時に消費されます）
void AddWaveSource(const WaveSource& source);

/// @brief コンピュートシェーダーを実行し、波のシミュレーションを1ステップ進めます
void UpdateInteractiveWater();

/// @brief 最新のハイトマップの GPU SRV ハンドルを取得します
D3D12_GPU_DESCRIPTOR_HANDLE GetInteractiveWaterHeightMap();

} // namespace RC
