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
    float2 shadowMapTexelSize; // 1テクセルのUVサイズ (1/width, 1/height)
    float pcfRadius;           // PCFのタップ間隔（テクセル単位。0以下なら1タップ＝PCF無効）
};

ConstantBuffer<ShadowData> gShadowParams : register(b6);
Texture2D<float> gShadowMap : register(t4);
SamplerState gShadowSampler : register(s2);              // s2 = Linear Clamp
SamplerComparisonState gShadowCmpSampler : register(s3); // s3 = 深度比較サンプラ (PCF用)

// ----------------------------------------------------------------------------
// PCF (Percentage Closer Filtering)
//   3x3 のタップを比較サンプラで取り、「光が当たっている率」を 0〜1 で返す。
//   比較サンプラ自体が 1 タップあたり 2x2 のバイリニア比較を行うため、
//   3x3 タップと合わせて実質 6x6 相当の滑らかさが得られる。
// ----------------------------------------------------------------------------
float SampleShadowPCF(float2 uv, float compareDepth)
{
    // PCF 無効時（半径0以下）は 1 タップだけ取る
    if (gShadowParams.pcfRadius <= 0.0f)
    {
        return gShadowMap.SampleCmpLevelZero(gShadowCmpSampler, uv, compareDepth);
    }

    float2 offsetStep = gShadowParams.shadowMapTexelSize * gShadowParams.pcfRadius;

    float litSum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 tapUV = uv + float2(x, y) * offsetStep;
            litSum += gShadowMap.SampleCmpLevelZero(gShadowCmpSampler, tapUV, compareDepth);
        }
    }

    return litSum / 9.0f;
}
