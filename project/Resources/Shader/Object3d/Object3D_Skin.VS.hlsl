#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// 行列パレット（SkinCluster）: T_i = InverseBindPose_i * SkeletonSpaceMatrix_i
StructuredBuffer<float4x4> gSkinMatrices : register(t1);

struct VertexShaderInput
{
    float4 position    : POSITION0;
    float2 texcoord    : TEXCOORD0;
    float3 normal      : NORMAL0;
    int4   boneIndex   : BLENDINDICES0;
    float4 boneWeight  : BLENDWEIGHT0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // Linear Blend Skinning: skinMat = Σ w_i * T_i
    float4x4 skinMat = (float4x4)0;
    skinMat += input.boneWeight.x * gSkinMatrices[input.boneIndex.x];
    skinMat += input.boneWeight.y * gSkinMatrices[input.boneIndex.y];
    skinMat += input.boneWeight.z * gSkinMatrices[input.boneIndex.z];
    skinMat += input.boneWeight.w * gSkinMatrices[input.boneIndex.w];

    // スキニング適用（モデルローカル空間 → スケルトン空間）
    float4 skinnedPos = mul(input.position, skinMat);
    float3 skinnedNormal = normalize(mul(input.normal, (float3x3)skinMat));

    // ワールド → クリップ空間
    output.position = mul(skinnedPos, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrix.worldInverseTranspose));
    output.worldPosition = mul(skinnedPos, gTransformationMatrix.World).xyz;
    output.instColor = float4(1, 1, 1, 1);
    return output;
}
