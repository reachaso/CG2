// ============================================================================
// EmitRain.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の雨エフェクト用 Emit Compute Shader。
// 上空の XZ 平面上にランダム配置し、下方向に落下させる。
//
// u0 : gParticles      — パーティクルデータ
// u1 : gFreeListIndex  — FreeList の現在のインデックス
// u2 : gFreeList       — 空きパーティクルインデックスの配列
// b0 : gPerFrame       — deltaTime 等
//
// Dispatch(emitCount / 1024, 1, 1) で実行。
// ============================================================================

struct Particle
{
    float3 translate;
    float pad0;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

cbuffer PerFrame : register(b0)
{
    float gDeltaTime;
    uint gMaxParticles;
    float2 gPadding;
};



RWStructuredBuffer<Particle> gParticles     : register(u0);
RWStructuredBuffer<int>      gFreeListIndex : register(u1);
RWStructuredBuffer<uint>     gFreeList      : register(u2);

// 簡易ハッシュ関数
float Hash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return float(seed) / 4294967295.0f;
}

float HashSigned(uint seed)
{
    return Hash(seed) * 2.0f - 1.0f;
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // FreeList のインデックスを1つ前に設定し、現在のインデックスを取得
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (0 <= freeListIndex && freeListIndex < (int)gMaxParticles)
    {
        // FreeList から空きパーティクルインデックスを取得
        uint particleIndex = gFreeList[freeListIndex];

        // ランダムシード
        uint seed = particleIndex * 2371u + DTid.x * 8923u + 17389u;

        // パーティクル初期化
        Particle p = (Particle)0;

        // 位置: 上空の広い XZ 平面上にランダム配置
        p.translate = float3(
            HashSigned(seed) * 10.0f,       // X: -10 ~ 10
            8.0f + Hash(seed + 1u) * 4.0f,  // Y: 8 ~ 12（上空）
            HashSigned(seed + 2u) * 10.0f    // Z: -10 ~ 10
        );

        // スケール: 雨粒（縦長の細い形状はVSで処理するが、ここでは小さめに）
        float s = 0.05f + Hash(seed + 3u) * 0.08f;
        p.scale = float3(s * 0.3f, s, s * 0.3f); // 縦長

        // 速度: 下方向 + わずかな横風
        p.velocity = float3(
            HashSigned(seed + 4u) * 0.005f,  // わずかな横風
            -(0.08f + Hash(seed + 5u) * 0.04f), // 下方向（負の Y）
            HashSigned(seed + 6u) * 0.005f   // わずかな横風
        );

        // 寿命: 中程度（落下時間）
        p.lifeTime = 3.0f + Hash(seed + 7u) * 3.0f;
        p.currentTime = 0.0f;

        // 色: 水色～薄い白
        float brightness = 0.7f + Hash(seed + 8u) * 0.3f;
        p.color = float4(
            brightness * 0.7f,
            brightness * 0.85f,
            brightness,
            0.6f + Hash(seed + 9u) * 0.4f // 半透明
        );

        gParticles[particleIndex] = p;
    }
    else
    {
        // 空きがないので、減らした分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
