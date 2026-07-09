#include "../Object3d/Object3d.hlsli"

struct InstanceData
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4 color;
};

StructuredBuffer<InstanceData> gInstances : register(t1);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID) {
    VSOutput output;
    InstanceData inst = gInstances[instanceId];
    float4 worldPos = mul(input.position, inst.World);
    output.position = mul(worldPos, gShadowParams.lightViewProjection);
    
    return output;
}
