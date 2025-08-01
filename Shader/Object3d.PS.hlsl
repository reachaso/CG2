#include "Object3d.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    int lightingMode; // 0:なし, 1:Lambert, 2:Half Lambert
    float3 padding; // アラインメント調整
    float4x4 uvTransform;
};

struct DirectionalLight
{
    
    float4 color; // ライトの色 (RGBA)
    float3 direction; // ライトの方向ベクトル (正規化されたベクトル)
    float intensity; // ライトの強度
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV 変換＆テクスチャサンプル
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float2 transformedUV = uv.xy;
    float4 textureColor = gTexture.Sample(gSampler, transformedUV);

    if (gMaterial.lightingMode != 0)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction.xyz);
        float lighting = 1.0f;
        if (gMaterial.lightingMode == 1)
        {
            lighting = max(NdotL, 0.0f); // Lambert
        }
        else if (gMaterial.lightingMode == 2)
        {
            lighting = NdotL * 0.5f + 0.5f; // Half Lambert
            lighting = lighting * lighting;
        }
        output.color =
        gMaterial.color
        * textureColor
        * gDirectionalLight.color
        * lighting
        * gDirectionalLight.intensity;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
