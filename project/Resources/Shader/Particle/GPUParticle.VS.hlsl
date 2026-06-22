// ============================================================================
// GPUParticle.VS.hlsl
// ----------------------------------------------------------------------------
// GPU Particle 用 Vertex Shader。
// Compute Shader で初期化・更新された StructuredBuffer<Particle> を参照し、
// billboard + scale + translate で worldMatrix を構築して WVP 変換を行う。
// ============================================================================

#include "GPUParticle.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    Particle particle = gParticles[instanceId];

    // 1. スケール適用 (ローカル座標)
    float3 localPos = input.position.xyz * particle.scale;

    // 2. ビルボード回転適用 (3x3行列として扱う)
    float3 rotatedPos = mul(localPos, (float3x3)gPerView.billboardMatrix);

    // 3. 平行移動適用 (ワールド座標へ)
    float3 worldPos = rotatedPos + particle.translate;

    // 4. ViewProjection変換
    output.position = mul(float4(worldPos, 1.0f), gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.color = particle.color;

    return output;
}
