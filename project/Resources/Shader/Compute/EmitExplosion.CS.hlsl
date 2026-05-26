// ============================================================================
// EmitExplosion.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の爆発エフェクト用 Emit Compute Shader。
// 全方向にランダム放射するパーティクルを射出する。
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
    float deltaTime;
    float3 perFramePad;
};

static const uint kMaxParticles = 1024;

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

// 球面上の均一ランダム方向ベクトルを生成
float3 RandomSphereDirection(uint seed)
{
    // cos(theta) を [-1, 1] でランダム、phi を [0, 2*PI] でランダム
    float cosTheta = HashSigned(seed);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = Hash(seed + 1u) * 6.28318530718f; // 2 * PI

    return float3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // FreeList のインデックスを1つ前に設定し、現在のインデックスを取得
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (0 <= freeListIndex && freeListIndex < (int)kMaxParticles)
    {
        // FreeList から空きパーティクルインデックスを取得
        uint particleIndex = gFreeList[freeListIndex];

        // ランダムシード
        uint seed = particleIndex * 1973u + DTid.x * 6547u + 13331u;

        // パーティクル初期化
        Particle p = (Particle)0;

        // 位置: 原点付近に集中
        p.translate = float3(
            HashSigned(seed + 10u) * 0.5f,
            Hash(seed + 11u) * 0.5f,
            HashSigned(seed + 12u) * 0.5f
        );

        // スケール: やや小さめ
        float s = 0.15f + Hash(seed + 3u) * 0.2f;
        p.scale = float3(s, s, s);

        // 速度: 球面上のランダム方向 × ランダム速さ
        float3 dir = RandomSphereDirection(seed + 20u);
        float speed = 0.03f + Hash(seed + 30u) * 0.06f;
        p.velocity = dir * speed;

        // 寿命: 短め（爆発なので）
        p.lifeTime = 1.0f + Hash(seed + 7u) * 2.0f;
        p.currentTime = 0.0f;

        // 色: オレンジ～赤のグラデーション
        float hue = Hash(seed + 40u);
        p.color = float4(
            1.0f,
            0.3f + hue * 0.5f,
            0.1f,
            1.0f
        );

        gParticles[particleIndex] = p;
    }
    else
    {
        // 空きがないので、減らした分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
