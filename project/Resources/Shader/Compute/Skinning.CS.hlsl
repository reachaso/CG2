// ============================================================================
// Skinning.CS.hlsl
// ----------------------------------------------------------------------------
// Compute Shader でスキニング（Linear Blend Skinning）を実行する。
//
// 入力:
//   t0 : gMatrixPalette  — スキニング行列パレット (IBP * SSM)
//   t1 : gInputVertices  — 元の頂点データ (position, texcoord, normal)
//   t2 : gInfluences     — 各頂点のボーンインデックスとウェイト
//   b0 : gSkinningInfo   — 頂点数
//
// 出力:
//   u0 : gOutputVertices — スキニング適用後の頂点データ
//
// スキニング済み頂点は position と normal のみ変換される。
// texcoord はそのままコピーされる。
// ============================================================================

// --- 頂点構造体（入出力共通） ---
// C++ 側の VertexData と同じレイアウト:
//   float4 position   (16 bytes)
//   float2 texcoord   ( 8 bytes)
//   float3 normal     (12 bytes)
//   int4   boneIndex  (16 bytes)  ← 入力にのみ存在
//   float4 boneWeight (16 bytes)  ← 入力にのみ存在
// 出力はスキニング済みなので boneIndex/boneWeight は不要だが、
// VBV のストライドを合わせるために同じ構造体で出力する。

struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
    int4   boneIndex;
    float4 boneWeight;
};

struct VertexInfluence
{
    int4   boneIndex;
    float4 boneWeight;
};

struct SkinningInformation
{
    uint numVertices;
};

// --- リソースバインド ---
StructuredBuffer<float4x4>        gMatrixPalette   : register(t0);
StructuredBuffer<Vertex>          gInputVertices   : register(t1);

RWStructuredBuffer<Vertex>        gOutputVertices  : register(u0);

ConstantBuffer<SkinningInformation> gSkinningInfo  : register(b0);

// --- エントリポイント ---
[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gSkinningInfo.numVertices)
    {
        return;
    }

    // 入力頂点の取得
    Vertex input = gInputVertices[vertexIndex];

    // Linear Blend Skinning: skinMat = Σ w_i * T_i
    float4x4 skinMat = (float4x4)0;
    skinMat += input.boneWeight.x * gMatrixPalette[input.boneIndex.x];
    skinMat += input.boneWeight.y * gMatrixPalette[input.boneIndex.y];
    skinMat += input.boneWeight.z * gMatrixPalette[input.boneIndex.z];
    skinMat += input.boneWeight.w * gMatrixPalette[input.boneIndex.w];

    // 出力を構築
    Vertex skinned;

    // スキニング適用（モデルローカル空間 → スケルトン空間）
    skinned.position = mul(input.position, skinMat);
    skinned.position.w = 1.0f; // w を保証

    // 法線にもスキニングを適用（回転のみ、正規化）
    skinned.normal = normalize(mul(input.normal, (float3x3)skinMat));

    // texcoord はそのままコピー
    skinned.texcoord = input.texcoord;

    // boneIndex/boneWeight はスキニング済みなので 0 クリア
    // （描画VSでは使わないが、ストライド合わせのために出力）
    skinned.boneIndex  = int4(0, 0, 0, 0);
    skinned.boneWeight = float4(0, 0, 0, 0);

    // 出力バッファに書き込み
    gOutputVertices[vertexIndex] = skinned;
}
