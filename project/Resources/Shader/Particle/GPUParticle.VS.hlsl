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

    // billboardMatrix をベースに worldMatrix を構築
    float4x4 worldMatrix = gPerView.billboardMatrix;

    // スケール適用: 各軸の方向ベクトルにスケールを乗算
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;

    // 平行移動: 4行目に translate を設定
    worldMatrix[3].xyz = particle.translate;

    // WVP 変換
    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;

    return output;
}
