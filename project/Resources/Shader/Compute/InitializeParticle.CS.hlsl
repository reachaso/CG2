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

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= gMaxParticles)
    {
        return;
    }

    // パーティクルを非表示状態（scale=0）で初期化
    gParticles[particleIndex] = (Particle)0;

    // FreeList を連番で初期化: [0, 1, 2, ..., gMaxParticles-1]
    gFreeList[particleIndex] = particleIndex;

    // スレッド 0 のみ: FreeListIndex を末尾に設定
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = (int)(gMaxParticles - 1);
    }
}
