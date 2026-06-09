// Water.hlsli - Shared structures between Water VS and PS

struct VertexShaderOutput {
    float4 position      : SV_POSITION;
    float2 texcoord      : TEXCOORD0;
    float3 normal        : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 instColor     : COLOR0;
};
