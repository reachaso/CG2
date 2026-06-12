// Water.VS.hlsl - Vertex Shader for ocean/water surface
// Uses Gerstner Waves for realistic vertex displacement

#include "Water.hlsli"

// b0: WVP 行列（VS用）
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// b6: 水面パラメータ (PS と共有、VS でも波の計算に使用)
cbuffer WaterParams : register(b6)
{
    float  gTime;         // 経過時間 (秒)
    float  gWaveHeight;   // 波の高さスケール
    float  gWaveSpeed;    // 波の速度スケール
    float  gWaveFreq;     // 波の周波数

    float  gWaveHeight2;  // 第2波の高さ
    float  gWaveSpeed2;   // 第2波の速度
    float  gWaveFreq2;    // 第2波の周波数
    float  gWaveSteepness; // Gerstner 鋭さ (0..1)

    float4 gWaterShallowColor;  // 浅瀬の色
    float4 gWaterDeepColor;     // 深海の色

    float  gFresnelPower;       // フレネル指数
    float  gSpecularPower;      // スペキュラ指数
    float  gNormalScrollSpeed;  // 法線マップスクロール速度
    float  gNormalStrength;     // 法線マップ強度
};

Texture2D<float> gInteractiveWave : register(t4);
SamplerState gSamplerClamp : register(s2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal   : NORMAL0;
};

// =====================================================
// Gerstner Wave
// =====================================================
// 2つの方向に異なる周波数の波を適用し、自然な海面を表現する
struct GerstnerResult
{
    float3 offset;
    float3 tangent;
    float3 binormal;
};

GerstnerResult ComputeGerstnerWave(float3 pos, float time)
{
    GerstnerResult result;
    result.offset = float3(0, 0, 0);
    result.tangent = float3(1, 0, 0);
    result.binormal = float3(0, 0, 1);

    // Wave 1: 主波 (X方向寄り)
    {
        float2 dir = normalize(float2(0.7, 0.5));
        float freq = gWaveFreq;
        float speed = gWaveSpeed * time;
        float steepness = gWaveSteepness;
        float amp = gWaveHeight;

        float d = dot(dir, pos.xz) * freq + speed;
        float s = sin(d);
        float c = cos(d);

        result.offset.x += steepness * amp * dir.x * c;
        result.offset.z += steepness * amp * dir.y * c;
        result.offset.y += amp * s;

        // 接線/従法線への影響
        result.tangent.x  -= steepness * dir.x * dir.x * freq * amp * s;
        result.tangent.y  += dir.x * freq * amp * c;
        result.binormal.z -= steepness * dir.y * dir.y * freq * amp * s;
        result.binormal.y += dir.y * freq * amp * c;
    }

    // Wave 2: 副波 (Z方向寄り, 位相をずらす)
    {
        float2 dir = normalize(float2(0.3, 0.8));
        float freq = gWaveFreq2;
        float speed = gWaveSpeed2 * time;
        float steepness = gWaveSteepness * 0.8;
        float amp = gWaveHeight2;

        float d = dot(dir, pos.xz) * freq + speed;
        float s = sin(d);
        float c = cos(d);

        result.offset.x += steepness * amp * dir.x * c;
        result.offset.z += steepness * amp * dir.y * c;
        result.offset.y += amp * s;

        result.tangent.x  -= steepness * dir.x * dir.x * freq * amp * s;
        result.tangent.y  += dir.x * freq * amp * c;
        result.binormal.z -= steepness * dir.y * dir.y * freq * amp * s;
        result.binormal.y += dir.y * freq * amp * c;
    }

    // Wave 3: 微細波（高周波のディテール）
    {
        float2 dir = normalize(float2(-0.4, 0.6));
        float freq = gWaveFreq * 2.5;
        float speed = gWaveSpeed * 1.3 * time;
        float steepness = gWaveSteepness * 0.3;
        float amp = gWaveHeight * 0.25;

        float d = dot(dir, pos.xz) * freq + speed;
        float s = sin(d);
        float c = cos(d);

        result.offset.x += steepness * amp * dir.x * c;
        result.offset.z += steepness * amp * dir.y * c;
        result.offset.y += amp * s;

        result.tangent.x  -= steepness * dir.x * dir.x * freq * amp * s;
        result.tangent.y  += dir.x * freq * amp * c;
        result.binormal.z -= steepness * dir.y * dir.y * freq * amp * s;
        result.binormal.y += dir.y * freq * amp * c;
    }

    return result;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // ワールド空間での頂点位置（波計算用）
    float4 worldPos = mul(input.position, gTransformationMatrix.World);

    // Gerstner Wave を計算
    GerstnerResult wave = ComputeGerstnerWave(worldPos.xyz, gTime);

    // インタラクティブ波紋のサンプリング
    // 20x20 の領域を対象とする
    float2 waveUV = (worldPos.xz / 20.0f) + 0.5f;
    float interactiveHeight = gInteractiveWave.SampleLevel(gSamplerClamp, waveUV, 0);

    // 波紋の法線計算のための有限差分
    float texel = 1.0f / 256.0f;
    float hL = gInteractiveWave.SampleLevel(gSamplerClamp, waveUV + float2(-texel, 0), 0);
    float hR = gInteractiveWave.SampleLevel(gSamplerClamp, waveUV + float2(texel, 0), 0);
    float hU = gInteractiveWave.SampleLevel(gSamplerClamp, waveUV + float2(0, -texel), 0);
    float hD = gInteractiveWave.SampleLevel(gSamplerClamp, waveUV + float2(0, texel), 0);
    
    // Y変位に対するX/Z方向の傾き
    // WorldPos = (x, y, z), scale is 20m, uv range 0~1.
    // dx = 20.0f * 2.0f * texel, dy = hR - hL
    float3 dX = float3(40.0f * texel, hR - hL, 0);
    float3 dZ = float3(0, hD - hU, 40.0f * texel);
    float3 interactiveNormal = normalize(cross(dZ, dX));

    // 変位適用
    worldPos.xyz += wave.offset;
    worldPos.y += interactiveHeight;

    // 法線を接線・従法線から計算（cross product）
    float3 gerstnerN = normalize(cross(wave.binormal, wave.tangent));
    
    // 法線の合成（簡易ブレンド：Y上向きを基準に足し合わせ）
    float3 N = normalize(float3(
        gerstnerN.x + interactiveNormal.x,
        gerstnerN.y * interactiveNormal.y,
        gerstnerN.z + interactiveNormal.z
    ));

    // 変位後のワールド位置を WVP で投影
    // World逆行列を使って元のローカルに戻してから WVP する代わりに、
    // 直接 View*Proj を適用するために、World の逆を避けて VP を使う
    float4x4 VP = mul(
        mul(float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1), gTransformationMatrix.WVP),
        float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
    );

    // 簡易的に: 元のローカル空間に offset を加えて WVP で変換
    float4 displacedLocal = input.position;
    displacedLocal.xyz += wave.offset;
    displacedLocal.y += interactiveHeight;
    output.position = mul(displacedLocal, gTransformationMatrix.WVP);

    output.texcoord = input.texcoord;
    output.normal = N;
    output.worldPosition = worldPos.xyz;
    output.instColor = float4(1, 1, 1, 1);

    return output;
}
