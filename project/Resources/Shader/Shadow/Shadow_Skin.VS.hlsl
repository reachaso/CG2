#include "../Object3d/Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 worldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

StructuredBuffer<float4x4> gSkinMatrices : register(t1);

struct VertexShaderInput
{
    float4 position    : POSITION0;
    float2 texcoord    : TEXCOORD0;
    float3 normal      : NORMAL0;
    int4   boneIndex   : BLENDINDICES0;
    float4 boneWeight  : BLENDWEIGHT0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VertexShaderInput input)
{
    VSOutput output;

    float4x4 skinMat = (float4x4)0;
    skinMat += input.boneWeight.x * gSkinMatrices[input.boneIndex.x];
    skinMat += input.boneWeight.y * gSkinMatrices[input.boneIndex.y];
    skinMat += input.boneWeight.z * gSkinMatrices[input.boneIndex.z];
    skinMat += input.boneWeight.w * gSkinMatrices[input.boneIndex.w];

    float4 skinnedPos = mul(input.position, skinMat);
    float4 worldPos = mul(skinnedPos, gTransformationMatrix.World);
    output.position = mul(worldPos, gShadowParams.lightViewProjection);

    return output;
}
