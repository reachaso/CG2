#include "Object3d.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    int enableLighting; // ライティングの有効化フラグ
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

    if (gMaterial.enableLighting != 0)
    { // ライティング有効時
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction.xyz);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        output.color =
            gMaterial.color
            * textureColor
            * gDirectionalLight.color
            * cos
            * gDirectionalLight.intensity;
    }
    else
    { // ライティング無効時
        output.color = gMaterial.color * textureColor;
    }

    return output;
}
