#include "Object3d.hlsli"

//struct TransformationMatrix {
//    float4x4 WVP;
//    float4x4 World;
//    float4x4 worldInverseTranspose;
//};

//ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

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

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    InstanceData inst = gInstances[instanceId];

    VertexShaderOutput output;
    output.position = mul(input.position, inst.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) inst.WorldInverseTranspose));
    output.worldPosition = mul(input.position, inst.World).xyz;
    output.instColor = inst.color;

    // ライトビュープロジェクションでライト空間の座標を計算
    output.lightSpacePos = mul(float4(output.worldPosition, 1.0f), gShadowParams.lightViewProjection);

    return output;
}
