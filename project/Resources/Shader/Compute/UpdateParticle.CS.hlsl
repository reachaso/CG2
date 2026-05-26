// ============================================================================
// UpdateParticle.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の毎フレーム更新用 Compute Shader。
// velocity による移動、deltaTime による寿命管理、alpha 減衰を行う。
// 寿命切れパーティクルは FreeList に返却する。
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

    // 速度による移動
    gParticles[particleIndex].translate += gParticles[particleIndex].velocity;

    // 経過時間の更新
    gParticles[particleIndex].currentTime += gDeltaTime;

    // 寿命に基づくアルファ減衰
    float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
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
