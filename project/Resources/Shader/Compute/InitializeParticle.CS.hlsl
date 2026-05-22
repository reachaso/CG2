// ============================================================================
// InitializeParticle.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の初期化用 Compute Shader。
// パーティクルバッファをゼロクリアし、FreeList を連番で初期化する。
//
// u0 : gParticles      — パーティクルデータ (RWStructuredBuffer<Particle>)
// u1 : gFreeListIndex  — FreeList の現在のインデックス (RWStructuredBuffer<int>)
// u2 : gFreeList       — 空きパーティクルインデックスの配列 (RWStructuredBuffer<uint>)
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

static const uint kMaxParticles = 1024;

RWStructuredBuffer<Particle> gParticles     : register(u0);
RWStructuredBuffer<int>      gFreeListIndex : register(u1);
RWStructuredBuffer<uint>     gFreeList      : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // パーティクルを非表示状態（scale=0）で初期化
    gParticles[particleIndex] = (Particle)0;

    // FreeList を連番で初期化: [0, 1, 2, ..., kMaxParticles-1]
    gFreeList[particleIndex] = particleIndex;

    // スレッド 0 のみ: FreeListIndex を末尾に設定
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = (int)(kMaxParticles - 1);
    }
}
