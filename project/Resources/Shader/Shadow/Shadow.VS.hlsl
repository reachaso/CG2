#include "../Object3d/Object3d.hlsli"

struct TransformationMatrix {
    matrix WVP;
    matrix World;
    matrix WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransform : register(b0);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VertexShaderInput input) {
    VSOutput output;
    // シャドウマップ描画時は、カメラの代わりにライト視点の WVP を使用する
    // 今回は Object3D 用の b0 (WVP) に WVP が入っているのではなく、World のみ入っている前提とすると
    // World 行列 × シャドウの lightViewProjection を使います。
    float4 worldPos = mul(input.position, gTransform.World);
    output.position = mul(worldPos, gShadowParams.lightViewProjection);
    
    return output;
}
