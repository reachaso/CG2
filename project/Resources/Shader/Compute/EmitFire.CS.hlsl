// ============================================================================
// EmitFire.CS.hlsl
// ----------------------------------------------------------------------------
// 炎 (ParticleType::Fire) 用の射出 Compute Shader。
// 手のひらなど「一点から立ち上がる炎」を想定し、
// 火種の面（XZ平面の円盤）から上方向へパーティクルを吹き上げる。
//
// Default (EmitParticle.CS) との違い:
//   - 発生位置を中心に寄せる（中心が密＝芯が明るくなる）
//   - 速度は常に上向きベース。横方向は小さなランダムだけ与える
//   - pad0 にパーティクル固有の乱数 (0〜1) を書き込み、
//     UpdateFire / FireParticle.PS でゆらぎの位相として使う
//
// u0 : gParticles      — パーティクルデータ
// u1 : gFreeListIndex  — FreeList の現在のインデックス
// u2 : gFreeList       — 空きパーティクルインデックスの配列
// b0 : gPerFrame       — エミッタパラメータ
//
// Dispatch(1, 1, 1) で emitCount スレッド（最大1024）実行。
// ============================================================================

struct Particle
{
    float3 translate;
    float pad0;      ///< 炎ではパーティクル固有の乱数 (0〜1) として使う
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

cbuffer PerFrame : register(b0)
{
    // 基本パラメータ (16 bytes)
    float gDeltaTime;
    uint gMaxParticles;
    float gMinLifeTime;
    float gMaxLifeTime;

    // スケール (16 bytes)
    float gMinScale;
    float gMaxScale;
    float gGravity;
    uint gEmitterShape;

    // 速度 (16 bytes)
    float3 gBaseVelocity;
    float gVelocityVariance;

    // 形状パラメータ (16 bytes)
    float gShapeRadius;
    float gConeAngle;
    float2 gShapePad;

    // 開始色 (16 bytes)
    float4 gStartColor;

    // 終了色 (16 bytes)
    float4 gEndColor;

    // エミッタ位置 (16 bytes)
    float3 gEmitterPosition;
    uint gEmitCount;

    // Box形状サイズ (16 bytes)
    float3 gShapeBoxSize;
    float gShapeBoxPad;
};

RWStructuredBuffer<Particle> gParticles     : register(u0);
RWStructuredBuffer<int>      gFreeListIndex : register(u1);
RWStructuredBuffer<uint>     gFreeList      : register(u2);

// 簡易ハッシュ関数（EmitParticle.CS.hlsl と同じもの）
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
    if (DTid.x >= gEmitCount)
    {
        return;
    }

    // FreeList のインデックスを1つ前に設定し、現在のインデックスを取得
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (0 <= freeListIndex && freeListIndex < (int)gMaxParticles)
    {
        uint particleIndex = gFreeList[freeListIndex];
        uint seed = particleIndex * 1973u + DTid.x * 6547u + 9277u;

        Particle p = (Particle)0;

        // --- 発生位置: 火種の円盤 (XZ平面) ---
        // 半径のランダムを pow(r, 1.5) で中心寄りに偏らせる。
        // 均一分布（sqrt）だと外周にパーティクルが集まって
        // ドーナツ状に見えてしまい、炎の「芯」ができない。
        float angle = Hash(seed) * 6.28318530f;
        float radius = gShapeRadius * pow(Hash(seed + 1u), 1.5f);
        p.translate = gEmitterPosition;
        p.translate.x += cos(angle) * radius;
        p.translate.z += sin(angle) * radius;
        // 根元が板状に見えないよう、縦にもわずかに散らす
        p.translate.y += HashSigned(seed + 2u) * gShapeRadius * 0.3f;

        // --- スケール ---
        float s = gMinScale + Hash(seed + 3u) * (gMaxScale - gMinScale);
        p.scale = float3(s, s, s);

        // --- 速度: 上向きベース + 横方向の小さなランダム ---
        // 横成分は gVelocityVariance をそのまま使わず 0.35 倍に抑える。
        // 横に強く散らすと炎が「煙」に見えるため、
        // 横は控えめにして UpdateFire のゆらぎで動きを付ける。
        p.velocity = gBaseVelocity;
        p.velocity.x += HashSigned(seed + 4u) * gVelocityVariance * 0.35f;
        p.velocity.z += HashSigned(seed + 5u) * gVelocityVariance * 0.35f;
        // 上向きだけは吹き上がりの強弱を付けたいので分散をそのまま乗せる
        p.velocity.y += Hash(seed + 6u) * gVelocityVariance;

        // --- 寿命 ---
        p.lifeTime = gMinLifeTime + Hash(seed + 7u) * (gMaxLifeTime - gMinLifeTime);
        p.currentTime = 0.0f;

        // --- 固有の乱数（ゆらぎの位相 / PS のちらつき用）---
        p.pad0 = Hash(seed + 8u);

        // --- 色: 開始色（以降 UpdateFire が endColor へ遷移させる）---
        p.color = gStartColor;

        gParticles[particleIndex] = p;
    }
    else
    {
        // 空きがないので、減らした分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
