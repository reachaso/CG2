#include "Object3d.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    int lightingMode; // 0:なし, 1:Lambert, 2:Half Lambert（※3/4が来てもLambert扱い）
    float shininess; // Blinn-Phongの指数（0以下なら鏡面なし）
    float environmentCoefficient; // 環境マップ映り込み係数 (0〜1)
    int useNormalMap;
    int useRoughnessMap;
    float2 padding; // 8 bytes padding to keep 16 bytes alignment (total size changed, but Wait, we need 12 bytes? No, lightingMode(4)+shininess(4)+env(4)+useN(4)+useR(4) = 20. + 12 = 32 bytes.)
    float4x4 uvTransform; // UV変換
};

ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float4 color;
    float3 direction; // 正規化済み想定
    float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float3 worldPosition;
};

ConstantBuffer<Camera> gCamera : register(b2);

struct PointLight
{
    float4 color;      // 光の色
    float3 position;   // 光の位置
    float intensity;   // 強度
    float radius;      // 届く距離（0以下なら無効）
    float decay;       // 減衰（指数）
    float2 padding;    // CBアラインメント調整
};

static const uint MAX_POINT_LIGHTS = 4;

cbuffer PointLightsCB : register(b3)
{
    uint pointCount;
    float3 _padPoint; // 16byte
    PointLight pointLights[MAX_POINT_LIGHTS];
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float2 padding;
};

static const uint MAX_SPOT_LIGHTS = 4;

cbuffer SpotLightsCB : register(b4)
{
    uint spotCount;
    float3 _padSpot; // 16byte
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};


struct AreaLight
{
    float4 color;
    float3 position;
    float intensity;

    float3 right;     // 面の右方向（正規化推奨）
    float halfWidth;

    float3 up;        // 面の上方向（正規化推奨）
    float halfHeight;

    float range;      // 影響距離（0以下なら無効）
    float decay;      // 減衰（指数）
    uint twoSided;    // 1なら両面
    uint pad;
};

static const uint MAX_AREA_LIGHTS = 4;

cbuffer AreaLightsCB : register(b5)
{
    uint areaCount;
    float3 _padArea; // 16byte
    AreaLight areaLights[MAX_AREA_LIGHTS];
};

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2D<float4> gNormalMap : register(t2);
Texture2D<float4> gRoughnessMap : register(t3);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV 変換＆テクスチャサンプル
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float2 transformedUV = uv.xy;
    float4 textureColor = gTexture.Sample(gSampler, transformedUV);

    // 2値抜き（必要なら閾値を調整）
    if (textureColor.a <= 0.5f /* || textureColor.a == 0.0f || output.color.a == 0.0f*/)
    {
        discard;
    }

    // ベース色（材質×テクスチャ）
    float4 base = gMaterial.color * textureColor;
    base *= input.instColor;

    // ライティングなし
    if (gMaterial.lightingMode == 0)
    {
        output.color = base;
        return output;
    }

    // =========================
    // Lambert / Half-Lambert + Blinn-Phong（固定）
    // =========================

    float3 N = normalize(input.normal);
    
    if (gMaterial.useNormalMap)
    {
        float4 normalMapCol = gNormalMap.Sample(gSampler, transformedUV);
        float3 normalTangentSpace = normalMapCol.xyz * 2.0f - 1.0f;
        
        // TBN 行列を計算
        float3 dp1 = ddx(input.worldPosition);
        float3 dp2 = ddy(input.worldPosition);
        float2 duv1 = ddx(transformedUV);
        float2 duv2 = ddy(transformedUV);

        float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
        float3 T = normalize((dp1 * duv2.y - dp2 * duv1.y) * r);
        float3 B = normalize((dp2 * duv1.x - dp1 * duv2.x) * r);
        
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(normalTangentSpace, TBN));
    }

    float currentShininess = gMaterial.shininess;
    if (gMaterial.useRoughnessMap && currentShininess > 0.0f)
    {
        float4 roughnessCol = gRoughnessMap.Sample(gSampler, transformedUV);
        float roughness = roughnessCol.r;
        currentShininess = max(0.1f, currentShininess * (1.0f - roughness));
        if (roughness > 0.99f) currentShininess = 0.0f;
    }

    // L は「面 → 光」方向に統一（Directionalは -direction でOK）
    float3 L = normalize(-gDirectionalLight.direction);

    // V は「面 → 視点」
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    float NdotL = dot(N, L);

    // 拡散（Lambert or Half-Lambert）
    float diffuseTerm = 0.0f;
    if (gMaterial.lightingMode == 2)
    {
        // Half-Lambert（暗部を持ち上げ）
        float h = saturate(NdotL * 0.5f + 0.5f);
        diffuseTerm = h * h;
    }
    else
    {
        diffuseTerm = saturate(NdotL);
    }

    float3 dirLightCol = gDirectionalLight.color.rgb * gDirectionalLight.intensity;

    // -----------------
    // Directional light
    // -----------------
    float3 diffuseDir = base.rgb * dirLightCol * diffuseTerm;

    float3 specularDir = 0.0f;
    if (currentShininess > 0.0f && NdotL > 0.0f) // 裏面にハイライト出さない
    {
        float3 H = normalize(L + V);
        float specPow = pow(saturate(dot(N, H)), currentShininess);
        specularDir = dirLightCol * specPow; // 白ハイライト
    }

// -----------------
// Point lights (MAX 4)
// -----------------
float3 diffusePoint = 0.0f;
float3 specularPoint = 0.0f;

[unroll]
for (uint i = 0; i < MAX_POINT_LIGHTS; ++i)
{
    if (i >= pointCount) { break; }

    PointLight pl = pointLights[i];

    if (pl.intensity <= 0.0f || pl.radius <= 0.0f) { continue; }

    float3 toLight = pl.position - input.worldPosition;
    float dist = length(toLight);

    if (dist >= pl.radius || dist <= 1e-5f) { continue; }

    float3 Lp = toLight / dist;

    // 減衰: (1 - d/r)^decay
    float t = saturate(1.0f - dist / pl.radius);
    float attenuation = pow(t, max(pl.decay, 0.0001f));

    float NdotLp = dot(N, Lp);

    float diffuseTermP = 0.0f;
    if (gMaterial.lightingMode == 2)
    {
        float h = saturate(NdotLp * 0.5f + 0.5f);
        diffuseTermP = h * h;
    }
    else
    {
        diffuseTermP = saturate(NdotLp);
    }

    float3 pointLightCol = pl.color.rgb * pl.intensity * attenuation;

    diffusePoint += base.rgb * pointLightCol * diffuseTermP;

    if (currentShininess > 0.0f && NdotLp > 0.0f)
    {
        float3 H = normalize(Lp + V);
        float specPow = pow(saturate(dot(N, H)), currentShininess);
        specularPoint += pointLightCol * specPow;
    }
}

// -----------------
// Spot lights (MAX 4)
// -----------------
float3 diffuseSpot = 0.0f;
float3 specularSpot = 0.0f;

[unroll]
for (uint i = 0; i < MAX_SPOT_LIGHTS; ++i)
{
    if (i >= spotCount) { break; }

    SpotLight sl = spotLights[i];

    if (sl.intensity <= 0.0f || sl.distance <= 0.0f) { continue; }

    float3 toLightS = sl.position - input.worldPosition;
    float distS = length(toLightS);

    if (distS >= sl.distance || distS <= 1e-5f) { continue; }

    float3 Ls = toLightS / distS;

    // 距離減衰: (1 - d/D)^decay
    float tS = saturate(1.0f - distS / sl.distance);
    float attenuationS = pow(tS, max(sl.decay, 0.0001f));

    // スポット係数（中心ほど強く、cosAngle以下で0）
    float3 dirN = normalize(sl.direction);
    float cosLD = dot(-Ls, dirN); // 光の向き(ライト→前) と (ライト→ピクセル) を比較
    float spot = saturate((cosLD - sl.cosAngle) / max(1e-5f, (1.0f - sl.cosAngle)));

    float3 spotLightCol = sl.color.rgb * sl.intensity * attenuationS * spot;

    float NdotLs = dot(N, Ls);

    float diffuseTermS = 0.0f;
    if (gMaterial.lightingMode == 2)
    {
        float h = saturate(NdotLs * 0.5f + 0.5f);
        diffuseTermS = h * h;
    }
    else
    {
        diffuseTermS = saturate(NdotLs);
    }

    diffuseSpot += base.rgb * spotLightCol * diffuseTermS;

    if (currentShininess > 0.0f && NdotLs > 0.0f)
    {
        float3 Hs = normalize(Ls + V);
        float specPowS = pow(saturate(dot(N, Hs)), currentShininess);
        specularSpot += spotLightCol * specPowS;
    }
}


// 合算
    
    // -----------------
    // Area light (Rect)  ※擬似: 四隅4サンプル
    // -----------------
    float3 diffuseArea = 0.0f;
    float3 specularArea = 0.0f;

    const uint aCount = min(areaCount, MAX_AREA_LIGHTS);
    [loop]
    for (uint ai = 0; ai < aCount; ++ai)
    {
        AreaLight al = areaLights[ai];
        if (al.intensity <= 0.0f || al.range <= 0.0f)
            continue;

        float3 R = al.right;
        float3 U = al.up;

        // 念のため正規化（0ベクトル回避）
        float rLen = length(R);
        float uLen = length(U);
        if (rLen < 1e-5f || uLen < 1e-5f)
            continue;
        R /= rLen;
        U /= uLen;

        float3 Ln = cross(R, U);
        float nLen = length(Ln);
        if (nLen < 1e-5f)
            continue;
        Ln /= nLen;

        float3 c = al.position;
        float hw = al.halfWidth;
        float hh = al.halfHeight;

        float3 corners[4] = {
            c + R * hw + U * hh,
            c + R * hw - U * hh,
            c - R * hw + U * hh,
            c - R * hw - U * hh
        };

        float3 areaLightColBase = al.color.rgb * al.intensity;

        // 4サンプル平均
        [unroll]
        for (int si = 0; si < 4; ++si)
        {
            float3 toL = corners[si] - input.worldPosition;
            float dist = length(toL);
            if (dist <= 1e-5f || dist >= al.range)
                continue;

            float3 La = toL / dist;

            // 片面発光チェック（twoSided==0 の時だけ）
            if (al.twoSided == 0)
            {
                // 点がライトの裏側ならスキップ
                if (dot(Ln, -La) <= 0.0f)
                    continue;
            }

            float t = saturate(1.0f - dist / al.range);
            float attenuation = pow(t, max(al.decay, 0.0001f));

            float NdotLa = dot(N, La);

            float diffuseTermA = 0.0f;
            if (gMaterial.lightingMode == 2)
            {
                float h = saturate(NdotLa * 0.5f + 0.5f);
                diffuseTermA = h * h;
            }
            else
            {
                diffuseTermA = saturate(NdotLa);
            }

            float3 areaLightCol = areaLightColBase * attenuation * 0.25f;

            diffuseArea += base.rgb * areaLightCol * diffuseTermA;

            if (currentShininess > 0.0f && NdotLa > 0.0f)
            {
                float3 H = normalize(La + V);
                float specPow = pow(saturate(dot(N, H)), currentShininess);
                specularArea += areaLightCol * specPow;
            }
        }
    }

// 合算
    output.color.rgb = (diffuseDir + diffusePoint + diffuseSpot + diffuseArea) + (specularDir + specularPoint + specularSpot + specularArea);

    // 環境マップ映り込み
    [branch]
    if (gMaterial.environmentCoefficient > 0.0f) {
        float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, N);
        float4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);
        output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
    }

    output.color.a = base.a;

    return output;
}
