// ============================================================================
// UpdateRain.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の雨エフェクト用 Update Compute Shader。
// 重力加速度を適用して落下させ、地面到達で FreeList に返却する。
//
// u0 : gParticles      — パーティクルデータ (RWStructuredBuffer)
// u1 : gFreeListIndex  — FreeList の現在のインデックス
// u2 : gFreeList       — 空きパーティクルインデックスの配列
// b0 : gPerFrame       — フレーム情報 (deltaTime)
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


static const float kGravity = -0.001f;   // 重力加速度（フレーム単位）
static const float kGroundY = -1.0f;     // 地面の高さ

RWStructuredBuffer<Particle> gParticles     : register(u0);
RWStructuredBuffer<int>      gFreeListIndex : register(u1);
RWStructuredBuffer<uint>     gFreeList      : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= gMaxParticles)
    {
        return;
    }

    // alpha が 0 のパーティクルは死んでいるとみなして更新しない
    if (gParticles[particleIndex].color.a == 0)
    {
        return;
    }

    // 重力加速度を速度に適用
    gParticles[particleIndex].velocity.y += kGravity;

    // 速度による移動
    gParticles[particleIndex].translate += gParticles[particleIndex].velocity;

    // 経過時間の更新
    gParticles[particleIndex].currentTime += gDeltaTime;

    // 寿命に基づくアルファ減衰
    float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);

    // 地面到達チェック（地面以下なら即座に消滅）
    if (gParticles[particleIndex].translate.y <= kGroundY)
    {
        alpha = 0.0f;
    }

    gParticles[particleIndex].color.a = saturate(alpha);

    // alpha が 0 になったら FreeList に返却
    if (gParticles[particleIndex].color.a == 0)
    {
        // スケールを 0 にして VertexShader 出力で棄却されるようにする
        gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);

        // FreeList に返却
        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

        // 最新の FreeListIndex の場所に死んだ ParticleIndex を設定
        if ((freeListIndex + 1) < (int)gMaxParticles)
        {
            gFreeList[freeListIndex + 1] = particleIndex;
        }
        else
        {
            // ここに来るはずはないが、安全策
            InterlockedAdd(gFreeListIndex[0], -1);
        }
    }
}
