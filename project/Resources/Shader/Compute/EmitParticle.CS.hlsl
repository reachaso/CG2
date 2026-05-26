// ============================================================================
// EmitParticle.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の射出用 Compute Shader。
// FreeList から空きインデックスを取得してパーティクルを初期化する。
//
// u0 : gParticles      — パーティクルデータ
// u1 : gFreeListIndex  — FreeList の現在のインデックス
// u2 : gFreeList       — 空きパーティクルインデックスの配列
// b0 : gPerFrame       — deltaTime 等
//
// Dispatch(1, 1, 1) で emitCount スレッド（最大1024）実行。
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
        uint seed = particleIndex * 1973u + DTid.x * 6547u + 9277u;

        // パーティクル初期化
        Particle p = (Particle)0;

        // 位置: 原点付近
        p.translate = float3(
            HashSigned(seed) * 2.0f,
            Hash(seed + 1u) * 1.0f,
            HashSigned(seed + 2u) * 2.0f
        );

        // スケール
        float s = 0.3f + Hash(seed + 3u) * 0.3f;
        p.scale = float3(s, s, s);

        // 速度: 上方向 + ランダム横揺れ
        p.velocity = float3(
            HashSigned(seed + 4u) * 0.02f,
            0.01f + Hash(seed + 5u) * 0.03f,
            HashSigned(seed + 6u) * 0.02f
        );

        // 寿命
        p.lifeTime = 3.0f + Hash(seed + 7u) * 5.0f;
        p.currentTime = 0.0f;

        // 色: 白
        p.color = float4(1.0f, 1.0f, 1.0f, 1.0f);

        gParticles[particleIndex] = p;
    }
    else
    {
        // 空きがないので、減らした分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
