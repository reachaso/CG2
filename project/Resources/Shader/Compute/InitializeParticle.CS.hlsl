// ============================================================================
// InitializeParticle.CS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle の初期化用 Compute Shader。
// DEFAULT ヒープ上の Particle バッファを 0 で初期化し、
// 確認用に scale と color を設定する。
//
// 入力/出力:
//   u0 : gParticles — パーティクルデータ (RWStructuredBuffer)
//
// Dispatch(1, 1, 1) で 1024 スレッド実行。
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

RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // 全要素を 0 で初期化
    gParticles[particleIndex] = (Particle)0;

    // 確認用: スケール 0.5、色を白に設定（見える状態）
    gParticles[particleIndex].scale = float3(0.5f, 0.5f, 0.5f);
    gParticles[particleIndex].color = float4(1.0f, 1.0f, 1.0f, 1.0f);
}
