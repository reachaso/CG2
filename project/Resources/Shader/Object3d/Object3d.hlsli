struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 instColor : COLOR0;
    float4 lightSpacePos : TEXCOORD1;
};

// ----------------------------------------------------------------------------
// Shadow Params
// ----------------------------------------------------------------------------
struct ShadowData {
    matrix lightViewProjection;
    float3 lightDirection;
    float bias;
    float4 color;
    int shadowMapEnabled;
    float3 pad;
};

ConstantBuffer<ShadowData> gShadowParams : register(b6);
Texture2D<float> gShadowMap : register(t4);
SamplerState gShadowSampler : register(s2); // s2 = Linear Clamp
