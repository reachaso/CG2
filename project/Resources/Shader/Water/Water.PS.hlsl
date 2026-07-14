// Water.PS.hlsl - Pixel Shader for ocean/water surface
// Fresnel reflection, scrolling normals, depth-based color

#include "Water.hlsli"

// b0: マテリアル（Object3D と同じレイアウト）
struct Material
{
    float4   color;
    int      lightingMode;
    float    shininess;
    float    environmentCoefficient;
    float    padding;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

// b1: ディレクショナルライト
struct DirectionalLight
{
    float4 color;
    float3 direction;
    float  intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

// b2: カメラ
struct Camera
{
    float3 worldPosition;
};

ConstantBuffer<Camera> gCamera : register(b2);

// b6: 水面パラメータ (VS と共有)
cbuffer WaterParams : register(b6)
{
    float  gTime;
    float  gWaveHeight;
    float  gWaveSpeed;
    float  gWaveFreq;

    float  gWaveHeight2;
    float  gWaveSpeed2;
    float  gWaveFreq2;
    float  gWaveSteepness;

    float4 gWaterShallowColor;
    float4 gWaterDeepColor;

    float  gFresnelPower;
    float  gSpecularPower;
    float  gNormalScrollSpeed;
    float  gNormalStrength;

    float4 gInvScreenSize;
    float4 gCameraNearFar; // x: near, y: far
    float4 gFoamParams;    // x: foamDepth, y: foamScale
    float4 gFoamColor;

    float4 gObstacles[4];  // xyz: pos, w: radius
    float4 gObstacleCount; // x: count
};

// t0: テクスチャ（法線マップとしても使用可能）
Texture2D<float4> gTexture : register(t0);

// t1: 環境キューブマップ (反射用)
TextureCube<float4> gEnvironmentTexture : register(t1);

// t4: インタラクティブ波紋ハイトマップ
Texture2D<float> gInteractiveWave : register(t4);

// t5: Foam用の深度テクスチャ
Texture2D<float> gDepthTexture : register(t5);

SamplerState gSampler : register(s0);
SamplerState gSamplerClamp : register(s2);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// =====================================================
// ノーマルマップから法線を取得（スクロール合成）
// =====================================================
float3 GetScrolledNormal(float2 uv, float time, float3 geometryNormal)
{
    float scrollSpeed = gNormalScrollSpeed;

    // 2層のUVスクロール（異なる速度・方向で重ね合わせ）
    float2 uv1 = uv * 4.0 + float2(scrollSpeed * time * 0.6, scrollSpeed * time * 0.4);
    float2 uv2 = uv * 6.0 + float2(-scrollSpeed * time * 0.3, scrollSpeed * time * 0.7);

    // テクスチャから法線マップを取得（0..1 → -1..1）
    float3 n1 = gTexture.Sample(gSampler, uv1).xyz * 2.0 - 1.0;
    float3 n2 = gTexture.Sample(gSampler, uv2).xyz * 2.0 - 1.0;

    // 2層の法線をブレンド
    float3 blended = normalize(n1 + n2);

    // 強度を調整
    blended.xy *= gNormalStrength;
    blended = normalize(blended);

    // 法線空間 → ワールド空間のマッピング（簡易版）
    // geometryNormal を up として、TBN を構築
    float3 N = normalize(geometryNormal);
    float3 T = normalize(cross(N, float3(0, 0, 1)));
    if (length(T) < 0.001)
        T = normalize(cross(N, float3(1, 0, 0)));
    float3 B = cross(N, T);

    return normalize(T * blended.x + B * blended.y + N * blended.z);
}

// =====================================================
// 深度値をリニアに変換（近・遠クリップ平面を使用）
// =====================================================
float LinearizeDepth(float depth, float nearZ, float farZ)
{
    // D3D12 (Z: 0 to 1, Reversed-Zではない標準プロジェクションを想定)
    // プロジェクションの設定によっては Reversed-Z を考慮する必要がある場合があります。
    // 今回は標準の投影として扱います。
    return (nearZ * farZ) / (farZ - depth * (farZ - nearZ));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // =========================
    // 深度計算（Zテスト代用＆岸からの距離）
    // =========================
    float2 screenUV = input.position.xy * gInvScreenSize.xy;
    float sceneDepthNonLinear = gDepthTexture.SampleLevel(gSamplerClamp, screenUV, 0).r;
    float sceneDepth = LinearizeDepth(sceneDepthNonLinear, gCameraNearFar.x, gCameraNearFar.y);
    float pixelDepth = LinearizeDepth(input.position.z, gCameraNearFar.x, gCameraNearFar.y);
    float depthDiff = sceneDepth - pixelDepth;

    if (depthDiff < 0.0f) {
        discard;
    }

    // 法線計算（ジオメトリ法線）
    float3 geoNormal = normalize(input.normal);

    // =========================
    // 跳ね返り波（法線のうねり）の計算
    // =========================
    // ddx, ddy を用いてスクリーン空間での水深の変化量とワールド座標の変化量を取得
    float dDepthX = ddx(depthDiff);
    float dDepthY = ddy(depthDiff);
    float3 dPosW_X = ddx(input.worldPosition);
    float3 dPosW_Y = ddy(input.worldPosition);

    // 水深が深くなる方向（岩から離れる方向＝沖）のワールド空間ベクトルを計算
    float3 depthGradient = normalize(dDepthX * dPosW_X + dDepthY * dPosW_Y + float3(0, 0.0001f, 0));
    depthGradient.y = 0.0f; // 水平方向のみにする
    depthGradient = normalize(depthGradient + float3(0, 0.0001f, 0));

    // 岸からの跳ね返り波の位相（泡の計算と同じ）
    float returnWaveFreq = 4.0f;
    float returnWaveSpeed = 3.5f;
    float returnWavePhase = depthDiff * returnWaveFreq + gTime * returnWaveSpeed;
    
    // 岸辺に近いほど波を強くする
    float foamDepthThreshold = max(gFoamParams.x, 0.001f);
    float foamFade = saturate(depthDiff / foamDepthThreshold);
    float foamBaseIntensity = pow(1.0f - foamFade, 1.5f);

    // 跳ね返り波による法線の傾き（cos波でうねりを表現）
    float returnSlope = cos(returnWavePhase) * 0.5f * foamBaseIntensity;
    
    // 跳ね返り方向（沖方向）に法線を傾ける
    float3 bounceNormal = depthGradient * returnSlope;

    // 波紋ハイトマップの法線計算
    float2 waveUV = (input.worldPosition.xz / 100.0f) + 0.5f;
    float texelSize = 1.0f / 256.0f;
    float hL = gInteractiveWave.Sample(gSamplerClamp, waveUV + float2(-texelSize, 0));
    float hR = gInteractiveWave.Sample(gSamplerClamp, waveUV + float2(texelSize, 0));
    float hD = gInteractiveWave.Sample(gSamplerClamp, waveUV + float2(0, -texelSize));
    float hU = gInteractiveWave.Sample(gSamplerClamp, waveUV + float2(0, texelSize));
    float3 interactiveNormal = normalize(float3(hL - hR, 2.0f * (100.0f * texelSize), hD - hU));
    
    // ジオメトリ法線 + 波紋法線 + 跳ね返り法線
    geoNormal = normalize(geoNormal + (interactiveNormal - float3(0, 1, 0)) + bounceNormal);

    // ノイズテクスチャのスクロール法線と合成
    float3 N = GetScrolledNormal(input.texcoord, gTime, geoNormal);

    // =========================
    // フレネル効果
    // =========================
    float NdotV = saturate(dot(N, V));
    // Schlick 近似: F = F0 + (1 - F0) * (1 - cosTheta)^5
    float F0 = 0.02; // 水の屈折率から算出される反射率
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, gFresnelPower);

    // =========================
    // 水の色（深度ベース）
    // =========================
    // フレネル値をそのまま深さの指標として使い、
    // 正面が浅瀬色、掠める角度が深海色にブレンド
    float4 waterColor = lerp(gWaterShallowColor, gWaterDeepColor, 1.0 - NdotV);
    waterColor *= gMaterial.color; // マテリアル色を乗算

    // =========================
    // 環境マップ反射
    // =========================
    float3 reflectedDir = reflect(-V, N);
    float4 envColor = gEnvironmentTexture.Sample(gSampler, reflectedDir);

    // =========================
    // ライティング (簡易)
    // =========================
    float3 L = normalize(-gDirectionalLight.direction);
    float NdotL = saturate(dot(N, L));

    // Half-Lambert
    float diffuse = saturate(NdotL * 0.5 + 0.5);
    diffuse *= diffuse;

    float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;

    // スペキュラ (Blinn-Phong)
    float3 H = normalize(L + V);
    float specular = pow(saturate(dot(N, H)), gSpecularPower) * gDirectionalLight.intensity;

    // =========================
    // 最終合成
    // =========================
    // 水面色（ディフューズ）+ 環境マップ反射 + スペキュラ
    float3 finalColor = waterColor.rgb * lightColor * diffuse;
    finalColor = lerp(finalColor, envColor.rgb, fresnel * gMaterial.environmentCoefficient);
    finalColor += lightColor * specular * 0.5;

    // =========================
    // 波打ち際（フォーム）の計算
    // =========================
 

    // =========================
    // 跳ね返り波（逆向きに広がる波紋）の計算
    // =========================
    // depthDiff (水深) を使って等深線に沿った波紋を作る。
    // gTime を足すことで、浅いところ(0)から深いところ(>0)へ向かって波紋が動く(跳ね返るように見える)。
    float returnWave = sin(returnWavePhase) * 0.5f + 0.5f; // 0 ~ 1

    // ノイズを使って泡の形を不規則にする
    // 少しゆっくり動くノイズUV
    float2 noiseUV = input.worldPosition.xz * 0.15f + float2(gTime * 0.05f, -gTime * 0.05f);
    // gTexture の r チャンネルをノイズとして借用
    float foamNoise = gTexture.Sample(gSampler, noiseUV).r;

    // 波打ち際全体の泡強度を合成（基本強度 × 跳ね返り波 × ノイズ）
    // さらに、岸スレスレ（foamFadeが0に近い部分）は常に白く残るようにする
    float dynamicFoam = foamBaseIntensity * returnWave * (foamNoise * 2.0f);
    float staticFoam = pow(1.0f - foamFade, 4.0f); // 岸の根本の強い白線
    float foamIntensity = saturate(dynamicFoam + staticFoam);

    // フォームの色を最終カラーに加算ブレンド
    float3 foamColor = gFoamColor.rgb * gFoamParams.y; // スケール適用
    finalColor = lerp(finalColor, foamColor, foamIntensity * gFoamColor.a);

    output.color = float4(finalColor, waterColor.a);

    return output;
}
