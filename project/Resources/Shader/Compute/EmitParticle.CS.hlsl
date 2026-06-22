// ============================================================================
// EmitParticle.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の射出用 Compute Shader。
// FreeList から空きインデックスを取得してパーティクルを初期化する。
// Particle Editor から渡されるパラメータに基づいて
// 寿命、サイズ、速度、色、エミッタ形状を設定する。
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
    float pad0;
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

// エミッタ形状ごとの初期位置を計算
float3 ComputeEmitPosition(uint seed)
{
    float3 pos = gEmitterPosition;

    if (gEmitterShape == 0) // Point
    {
        // 一点から（位置変化なし）
    }
    else if (gEmitterShape == 1) // Sphere
    {
        // 球面上のランダムな位置
        float theta = Hash(seed) * 6.28318530f;
        float phi = acos(HashSigned(seed + 1u));
        float r = gShapeRadius * pow(Hash(seed + 2u), 1.0f / 3.0f);
        pos.x += r * sin(phi) * cos(theta);
        pos.y += r * sin(phi) * sin(theta);
        pos.z += r * cos(phi);
    }
    else if (gEmitterShape == 2) // Box
    {
        pos.x += HashSigned(seed) * gShapeBoxSize.x * 0.5f;
        pos.y += HashSigned(seed + 1u) * gShapeBoxSize.y * 0.5f;
        pos.z += HashSigned(seed + 2u) * gShapeBoxSize.z * 0.5f;
    }
    else if (gEmitterShape == 3) // Cone
    {
        float angle = Hash(seed) * 6.28318530f;
        float radiusFactor = Hash(seed + 1u);
        float halfAngle = gConeAngle;
        float spreadRadius = tan(halfAngle) * radiusFactor * gShapeRadius;
        pos.x += cos(angle) * spreadRadius;
        pos.z += sin(angle) * spreadRadius;
    }

    return pos;
}

// エミッタ形状ごとの初速を計算
float3 ComputeEmitVelocity(uint seed)
{
    float3 vel = gBaseVelocity;

    if (gEmitterShape == 0) // Point
    {
        vel.x += HashSigned(seed) * gVelocityVariance;
        vel.y += HashSigned(seed + 1u) * gVelocityVariance;
        vel.z += HashSigned(seed + 2u) * gVelocityVariance;
    }
    else if (gEmitterShape == 1) // Sphere
    {
        // 放射状
        float theta = Hash(seed) * 6.28318530f;
        float phi = acos(HashSigned(seed + 1u));
        float speed = length(gBaseVelocity) + HashSigned(seed + 2u) * gVelocityVariance;
        vel.x = sin(phi) * cos(theta) * speed;
        vel.y = sin(phi) * sin(theta) * speed;
        vel.z = cos(phi) * speed;
    }
    else if (gEmitterShape == 2) // Box
    {
        vel.x += HashSigned(seed) * gVelocityVariance;
        vel.y += HashSigned(seed + 1u) * gVelocityVariance;
        vel.z += HashSigned(seed + 2u) * gVelocityVariance;
    }
    else if (gEmitterShape == 3) // Cone
    {
        float angle = Hash(seed) * 6.28318530f;
        float spread = Hash(seed + 1u) * gConeAngle;
        float speed = length(gBaseVelocity) + HashSigned(seed + 2u) * gVelocityVariance;
        vel.x = sin(spread) * cos(angle) * speed;
        vel.y = cos(spread) * speed;
        vel.z = sin(spread) * sin(angle) * speed;
    }

    return vel;
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
        // FreeList から空きパーティクルインデックスを取得
        uint particleIndex = gFreeList[freeListIndex];

        // ランダムシード
        uint seed = particleIndex * 1973u + DTid.x * 6547u + 9277u;

        // パーティクル初期化
        Particle p = (Particle)0;

        // 位置: エミッタ形状に基づく
        p.translate = ComputeEmitPosition(seed + 100u);

        // スケール: パラメータ範囲
        float s = gMinScale + Hash(seed + 3u) * (gMaxScale - gMinScale);
        p.scale = float3(s, s, s);

        // 速度: エミッタ形状に基づく
        p.velocity = ComputeEmitVelocity(seed + 200u);

        // 寿命: パラメータ範囲
        p.lifeTime = gMinLifeTime + Hash(seed + 7u) * (gMaxLifeTime - gMinLifeTime);
        p.currentTime = 0.0f;

        // 色: 開始色
        p.color = gStartColor;

        gParticles[particleIndex] = p;
    }
    else
    {
        // 空きがないので、減らした分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
