// ============================================================================
// GPUParticle.hlsli
// ----------------------------------------------------------------------------
// GPU Particle 用の共通構造体定義
// ============================================================================

struct Particle
{
    float3 translate;
    float pad0;      ///< 予備領域。ParticleType::Fire では固有の乱数 (0〜1) として使う
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

struct PerView
{
    row_major float4x4 viewProjection;
    row_major float4x4 billboardMatrix;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    /// @brief パーティクル固有の情報
    /// x = 寿命の進行度 (0=生成直後, 1=消滅直前)
    /// y = パーティクル固有の乱数 (0〜1)。Particle.pad0 の値
    /// @details 炎のように「経過時間で形やちらつきを変えたい」PS のために
    ///          VS から渡す。使わない PS は無視すればよい。
    float2 particleParams : TEXCOORD1;
};
