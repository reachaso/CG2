// ============================================================================
// BubbleParticle.PS.hlsl
// ----------------------------------------------------------------------------
// 泡パーティクル用のPixel Shader。
// UVから中心距離を計算し、縁を白く不透明に、中心を透明に描画します。
// ============================================================================

#include "GPUParticle.hlsli"

Texture2D<float4> gTexture : register(t0); // テクスチャは指定されても基本無視するか、歪み用などで使える
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV座標から中心(0.5, 0.5)への距離を計算
    float2 center = float2(0.5f, 0.5f);
    float dist = distance(input.texcoord, center);
    
    // 半径0.5より外側は描画しない（完全な円にする）
    if (dist > 0.5f)
    {
        discard;
    }

    // 泡の縁の計算
    // dist が 0.0(中心) 〜 0.5(外周) に分布する。
    // dist を 0.0〜1.0 に正規化
    float normalizedDist = dist * 2.0f;
    
    // 縁の太さとシャープさの調整
    // 1.0に近いほど縁。powでカーブをつけて急激に不透明にする
    float edgeFactor = pow(normalizedDist, 4.0f);
    
    // ハイライト（左上からの光を想定して少しずらした位置を明るくする）
    float2 highlightOffset = float2(0.35f, 0.35f);
    float highlightDist = distance(input.texcoord, highlightOffset);
    float highlightFactor = saturate(1.0f - highlightDist * 5.0f);
    highlightFactor = pow(highlightFactor, 2.0f);

    // 最終的なアルファ値
    // 縁の部分 + ハイライト部分 + 基本色(input.color)のアルファ
    float alpha = saturate(edgeFactor + highlightFactor);
    alpha *= input.color.a;

    // 泡の基本色は白ベースだが、設定色(input.color)も乗算して色付き泡も可能にする
    // 縁とハイライトは白に近づける
    float3 baseColor = lerp(input.color.rgb, float3(1.0f, 1.0f, 1.0f), edgeFactor * 0.8f);
    baseColor += float3(1.0f, 1.0f, 1.0f) * highlightFactor; // ハイライトの加算

    output.color = float4(saturate(baseColor), alpha);

    if (output.color.a < 0.01f)
    {
        discard;
    }

    return output;
}
