// ============================================================================
// Object3D_WaterBall.PS.hlsl
// ----------------------------------------------------------------------------
// Water ball pixel shader for water bullets / splash particles.
// Features:
//   - Procedural noise-based surface distortion (no time needed, uses worldPos)
//   - Fresnel reflection (bright edges like real water)
//   - Environment map reflection
//   - Subsurface scattering approximation (light passes through water)
//   - Premultiplied alpha output for proper transparency compositing
// ============================================================================

#include "Object3d.hlsli"

// ---------- Constant Buffers (same layout as Object3D / Glass) ----------

struct Material
{
    float4 color;
    int lightingMode;
    float shininess;
    float environmentCoefficient;
    float padding;
    float4x4 uvTransform;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float4 color;
    float3 direction;
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
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay;
    float2 padding;
};
static const uint MAX_POINT_LIGHTS = 4;
cbuffer PointLightsCB : register(b3)
{
    uint pointCount;
    float3 _padPoint;
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
    float3 _padSpot;
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

struct AreaLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 right;
    float halfWidth;
    float3 up;
    float halfHeight;
    float range;
    float decay;
    uint twoSided;
    uint pad;
};
static const uint MAX_AREA_LIGHTS = 4;
cbuffer AreaLightsCB : register(b5)
{
    uint areaCount;
    float3 _padArea;
    AreaLight areaLights[MAX_AREA_LIGHTS];
};

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ==========================================================================
// Utility functions
// ==========================================================================

// Simple hash-based pseudo-random (deterministic, no time needed)
float hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

// 3D value noise
float noise3D(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep

    float n000 = hash31(i + float3(0, 0, 0));
    float n100 = hash31(i + float3(1, 0, 0));
    float n010 = hash31(i + float3(0, 1, 0));
    float n110 = hash31(i + float3(1, 1, 0));
    float n001 = hash31(i + float3(0, 0, 1));
    float n101 = hash31(i + float3(1, 0, 1));
    float n011 = hash31(i + float3(0, 1, 1));
    float n111 = hash31(i + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);

    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);

    return lerp(nxy0, nxy1, f.z);
}

// Fractional Brownian Motion (3 octaves) for surface detail
float fbm3(float3 p)
{
    float val = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 3; ++i)
    {
        val += amp * noise3D(p * freq);
        freq *= 2.17;
        amp *= 0.5;
    }
    return val;
}

// Schlick Fresnel
float FresnelSchlick(float cosTheta, float F0)
{
    float x = 1.0 - saturate(cosTheta);
    float x2 = x * x;
    float x5 = x2 * x2 * x;
    return F0 + (1.0 - F0) * x5;
}

// ==========================================================================
// Main
// ==========================================================================

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV
    float4 uv = mul(float4(input.texcoord, 0.0, 1.0), gMaterial.uvTransform);
    float4 tex = gTexture.Sample(gSampler, uv.xy);
    float4 base = gMaterial.color * tex * input.instColor;

    if (base.a <= 0.001)
        discard;

    // ==================
    // Water color tint
    // ==================
    // Override base color with a rich water blue-cyan palette
    float3 waterDeep   = float3(0.02, 0.15, 0.45);  // Deep ocean blue
    float3 waterBright = float3(0.15, 0.55, 0.95);   // Bright water surface
    float3 waterFoam   = float3(0.6, 0.85, 1.0);     // Foam/highlight color

    // Use world position for procedural noise pattern
    float3 wp = input.worldPosition;
    float noiseVal = fbm3(wp * 8.0); // High frequency detail

    // Mix between deep and bright water based on noise
    float3 waterColor = lerp(waterDeep, waterBright, noiseVal);

    // Add subtle foam-like highlights at peaks
    float foamMask = smoothstep(0.55, 0.75, noiseVal);
    waterColor = lerp(waterColor, waterFoam, foamMask * 0.4);

    // Apply material tint (user can shift color via material.color)
    waterColor *= base.rgb * 2.5; // Boost to compensate for dark base

    // ==================
    // Vectors
    // ==================
    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // Perturb normal with noise for ripple effect
    float eps = 0.02;
    float nx = fbm3((wp + float3(eps, 0, 0)) * 8.0) - fbm3((wp - float3(eps, 0, 0)) * 8.0);
    float ny = fbm3((wp + float3(0, eps, 0)) * 8.0) - fbm3((wp - float3(0, eps, 0)) * 8.0);
    float nz = fbm3((wp + float3(0, 0, eps)) * 8.0) - fbm3((wp - float3(0, 0, eps)) * 8.0);
    float3 noiseNormal = normalize(float3(nx, ny, nz));
    
    // Blend noise normal with geometry normal
    N = normalize(N + noiseNormal * 0.35);

    if (dot(N, V) < 0.0)
        N = -N;

    float NdotV = saturate(dot(N, V));

    // ==================
    // Fresnel (water IOR ~1.33)
    // ==================
    float ior = 1.33;
    float f = (1.0 - ior) / (1.0 + ior);
    float F0 = f * f; // ~0.02 for water
    float fresnel = FresnelSchlick(NdotV, F0);

    // Water is smooth: high shininess
    float shininess = max(gMaterial.shininess, 512.0);

    // ==================
    // Lighting
    // ==================
    float3 diffuseSum = 0.0;
    float3 specularSum = 0.0;

    // Directional light
    {
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = dot(N, L);
        float diffuseTerm = saturate(NdotL * 0.5 + 0.5);
        diffuseTerm *= diffuseTerm;

        float3 lightCol = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
        diffuseSum += waterColor * lightCol * diffuseTerm;

        if (NdotL > 0.0)
        {
            float3 H = normalize(L + V);
            float specPow = pow(saturate(dot(N, H)), shininess);
            specularSum += lightCol * specPow;
        }
    }

    // Point lights
    [unroll]
    for (uint i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        if (i >= pointCount)
            break;
        PointLight pl = pointLights[i];
        if (pl.intensity <= 0.0 || pl.radius <= 0.0)
            continue;

        float3 toL = pl.position - input.worldPosition;
        float dist = length(toL);
        if (dist >= pl.radius || dist <= 1e-5)
            continue;

        float3 L = toL / dist;
        float t = saturate(1.0 - dist / pl.radius);
        float atten = pow(t, max(pl.decay, 0.0001));

        float NdotL = dot(N, L);
        float diffuseTerm = saturate(NdotL * 0.5 + 0.5);
        diffuseTerm *= diffuseTerm;

        float3 lightCol = pl.color.rgb * pl.intensity * atten;
        diffuseSum += waterColor * lightCol * diffuseTerm;

        if (NdotL > 0.0)
        {
            float3 H = normalize(L + V);
            float specPow = pow(saturate(dot(N, H)), shininess);
            specularSum += lightCol * specPow;
        }
    }

    // Spot lights
    [unroll]
    for (uint j = 0; j < MAX_SPOT_LIGHTS; ++j)
    {
        if (j >= spotCount)
            break;
        SpotLight sl = spotLights[j];
        if (sl.intensity <= 0.0 || sl.distance <= 0.0)
            continue;

        float3 toL = sl.position - input.worldPosition;
        float dist = length(toL);
        if (dist >= sl.distance || dist <= 1e-5)
            continue;

        float3 L = toL / dist;
        float t = saturate(1.0 - dist / sl.distance);
        float atten = pow(t, max(sl.decay, 0.0001));

        float3 dirN = normalize(sl.direction);
        float cosLD = dot(-L, dirN);
        float spot = saturate((cosLD - sl.cosAngle) / max(1e-5, (1.0 - sl.cosAngle)));

        float3 lightCol = sl.color.rgb * sl.intensity * atten * spot;

        float NdotL = dot(N, L);
        float diffuseTerm = saturate(NdotL * 0.5 + 0.5);
        diffuseTerm *= diffuseTerm;

        diffuseSum += waterColor * lightCol * diffuseTerm;

        if (NdotL > 0.0)
        {
            float3 H = normalize(L + V);
            float specPow = pow(saturate(dot(N, H)), shininess);
            specularSum += lightCol * specPow;
        }
    }

    // ==================
    // Environment reflection
    // ==================
    float3 cameraToSurface = normalize(input.worldPosition - gCamera.worldPosition);
    float3 reflectedVec = reflect(cameraToSurface, N);
    float4 envColor = gEnvironmentTexture.Sample(gSampler, reflectedVec);

    // ==================
    // Subsurface scattering approximation
    // ==================
    float3 L_dir = normalize(-gDirectionalLight.direction);
    float sss = saturate(dot(V, -L_dir)) * 0.3;
    float3 subsurface = waterBright * gDirectionalLight.color.rgb * gDirectionalLight.intensity * sss;

    // ==================
    // Composite
    // ==================
    // Reflection (Fresnel-weighted: edges reflect more)
    float3 reflection = specularSum * (0.3 + 2.5 * fresnel);
    reflection += envColor.rgb * fresnel * 0.8;

    // Water opacity: semi-transparent, edges become more opaque (Fresnel)
    float opacity = saturate(base.a * 0.7 + fresnel * 0.5);

    // Inner glow: subtle bright core
    float innerGlow = smoothstep(0.0, 0.6, NdotV) * 0.15;
    float3 glowColor = waterBright * innerGlow;

    // Final color (premultiplied alpha)
    float3 finalColor = diffuseSum + subsurface + glowColor;
    output.color.rgb = saturate(finalColor * opacity + reflection);
    output.color.a = opacity;

    return output;
}
