#include "Object3d.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    int enableLighting; // ライティングの有効化フラグ
    float padding[3];
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
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    if (gMaterial.enableLighting != 0) { // ライティング有効時
    // コサイン項を計算（入力法線は正規化しておく）
        float cosTerm = saturate(
    dot(
        normalize(input.normal),
        -gDirectionalLight.direction.xyz
    )
);
    // マテリアル色 × テクスチャ色 × 光の色 × コサイン項 × 光強度
        output.color =
        gMaterial.color 
      * textureColor
      * gDirectionalLight.color 
      * cosTerm
      * gDirectionalLight.intensity;
    }
    else
    { // ライティング無効時（前回までと同じ演算）
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}
