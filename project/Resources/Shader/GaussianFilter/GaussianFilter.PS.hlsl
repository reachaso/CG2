#include "../Fullscreen/Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// PostEffectParams (RootConstants b0)
cbuffer PostEffectParams : register(b0) {
    uint gKernelRadius; // カーネル半径 k
    float gSigma;       // 標準偏差
    uint pad0;
    uint pad1;
};

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

PixelShaderOutPut main(VertexShaderOutput input) {
    PixelShaderOutPut output;

    // 1. uvStepSizeを算出
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp((float32_t)width), rcp((float32_t)height));

    // 2. カーネル半径とシグマ
    int32_t k = (int32_t)gKernelRadius;
    float32_t sigma = gSigma;

    if (sigma <= 0.0f) {
        sigma = 0.0001f;
    }

    // 3. ループを回す
    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t weightSum = 0.0f;

    for (int32_t x = -k; x <= k; ++x) {
        for (int32_t y = -k; y <= k; ++y) {
            // 現在のtexcoordを算出
            float32_t2 texcoord = input.texcoord + float32_t2((float32_t)x, (float32_t)y) * uvStepSize;
            float32_t3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
            
            // ガウス関数の重みを計算
            float32_t weight = exp(-((float32_t)(x * x + y * y)) / (2.0f * sigma * sigma));
            
            color += fetchColor * weight;
            weightSum += weight;
        }
    }

    // 4. 正規化して出力
    output.color.rgb = color / weightSum;
    output.color.a = 1.0f;

    return output;
}
