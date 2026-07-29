struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// b1: ScreenDroplets Parameters (Total: 32 bytes = float4 x 2)
cbuffer ScreenDropletsParams : register(b1) {
    float time;          // 経過時間
    float intensity;     // 全体強度・消滅フェード (0.0 ~ 1.0)
    float speed;         // 落下速度
    float distortion;    // 屈折・歪み強度
    float scale;         // 水滴グリッドスケール (密度・サイズ)
    float aspectRatio;   // アスペクト比 (Width / Height)
    float2 _padding;     // 16バイトアライメント補正
};

// 2D座標からの疑似乱数
float Hash21(float2 p) {
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

// 1層分の水滴・気泡を計算する関数
// 戻り値: float4(x: 屈折X, y: 屈折Y, z: スペキュラ光沢ハイライト, w: フチの立体感陰影)
float4 ComputeDropletLayer(float2 uv, float colScale, float layerSpeed, float timeVal, float intensityVal) {
    // アスペクト比を反映したX座標（画面の比率が変わっても正円形状を維持）
    float aspectX = uv.x * aspectRatio;
    
    // 縦レーン構造（列分割）の採用：同一レーン内でのみスクロールを行うため、途中で切れたり消えたりしない
    float colIndex = floor(aspectX * colScale);
    float colSeed = Hash21(float2(colIndex, colIndex * 1.35f + 42.1f));
    
    // 【適量の泡と水滴への増量調整】
    // レーン非アクティブ率を以前の72%から55%に引き下げ、適度な賑やかさと豊富な気泡感を引き出します。
    if (colSeed < 0.55f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    // レーン内でのXローカル座標 [-0.5, 0.5]
    float localX = frac(aspectX * colScale) - 0.5f;
    
    // 速度（正：重力に沿って下に向かって落ちる / 負：浮力・潜水により上に向かって昇る）
    float absSpeed = abs(layerSpeed);
    float speedSign = sign(layerSpeed);
    float dropSpeed = (0.28f + (colSeed - 0.55f) * 2.0f) * absSpeed * speedSign;
    float colOffset = Hash21(float2(colIndex * 7.1f, 19.3f)) * 100.0f;
    
    // 【上下方向スクロールの是正】
    // - timeVal * dropSpeed とすることで、speed > 0 なら下へ滴り、speed < 0 なら上へ昇る！
    float scrollY = (uv.y * colScale) - timeVal * dropSpeed + colOffset;
    
    // レーン内の各セルの縦ID
    float rowIndex = floor(scrollY);
    float rowSeed = Hash21(float2(colIndex, rowIndex));
    
    // 上下方向の間引き率も以前の45%から25%まで引き下げ、連なる泡と水滴の豊かさを表現
    if (rowSeed < 0.25f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    // セル内でのYローカル座標 [-0.5, 0.5]
    float localY = frac(scrollY) - 0.5f;
    
    // クリッピング（欠けや消滅）防止：セル中心近く [-0.14, 0.14] に安全収容
    float xShift = (Hash21(float2(colIndex, rowIndex * 3.1f)) - 0.5f) * 0.24f;
    float yShift = (Hash21(float2(rowIndex, colIndex * 2.7f)) - 0.5f) * 0.24f;
    
    float2 dropPos = float2(localX - xShift, localY - yShift);
    
    // 水滴半径の算出（0.12 ~ 0.20 のクリップされない安全大粒・中粒サイズ）
    float baseRadius = 0.12f + (rowSeed * 0.09f);
    float radius = baseRadius * sqrt(max(0.0f, intensityVal));
    if (radius <= 0.001f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    
    // 気泡・水滴の両方に似合う、優雅なラウンドシルエット
    float2 dropUV = dropPos;
    dropUV.y *= 1.04f;
    
    float dist = length(dropUV);
    float r = dist / radius; // 水滴内部の中心0.0 ～ フチ1.0
    
    float2 norm = float2(0.0f, 0.0f);
    float glint = 0.0f;
    float shadow = 0.0f;
    
    // 1. 半球ドーム状レンズの物理的屈折と光沢ハイライト
    if (r < 1.0f) {
        float z = sqrt(max(0.001f, 1.0f - r * r));
        
        // 球面ガラス屈折
        norm = (dropUV / radius) * z * 1.6f;
        
        // スペキュラ光沢（斜め左上方からの入射光シャイン反射）
        float3 lightDir = normalize(float3(-0.35f, 0.65f, 0.67f));
        float3 normal3D = normalize(float3(-dropUV.x / radius, -dropUV.y / radius, z));
        float spec = pow(max(0.0f, dot(normal3D, lightDir)), 50.0f);
        
        // 柔らかな輪郭ハイライト
        float topRim = smoothstep(0.35f, 0.95f, -dropUV.y / radius) * smoothstep(1.0f, 0.55f, r) * 0.35f;
        
        glint = (spec * 4.2f + topRim) * intensityVal;
        
        // フチ部 (r: 0.75 ~ 1.0) に生じる立体陰影
        shadow = smoothstep(0.7f, 0.98f, r) * smoothstep(1.0f, 0.94f, r) * 0.55f * intensityVal;
    }
    
    return float4(norm.x, norm.y, glint, shadow) * intensityVal;
}

PixelShaderOutput main(PixelShaderInput input) {
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    
    if (intensity <= 0.001f) {
        output.color = gTexture.Sample(gSampler, uv);
        return output;
    }
    
    // 高さを異にする2つの適正スケールの層（主大粒層＋副中粒層）を重ね、自然で奥行きあるレンズ水滴を作り出します。
    float4 layer1 = ComputeDropletLayer(uv, scale * 2.0f, speed * 0.8f, time, intensity);
    float4 layer2 = ComputeDropletLayer(uv + float2(0.41f, 0.59f), scale * 2.8f, speed * 1.15f, time * 1.1f, intensity * 0.85f);
    
    // 法線屈折ベクトル、シャインハイライト、フチ立体陰影の合成
    float2 totalNorm = layer1.xy + layer2.xy * 0.7f;
    float totalGlint = (layer1.z + layer2.z) * sqrt(max(0.0f, intensity));
    float totalShadow = (layer1.w + layer2.w);

    // ドーム球面屈折によって背景テクスチャの座標を滑らかに引く
    float2 distortedUV = clamp(uv + totalNorm * distortion, 0.0f, 1.0f);
    
    // 屈折座標で背景画像を取得
    float4 sceneColor = gTexture.Sample(gSampler, distortedUV);
    
    // 水滴境目に輪郭和らぐ立体感を出すため、わずかな陰影を付与
    sceneColor.rgb *= lerp(1.0f, 0.70f, clamp(totalShadow * 1.8f, 0.0f, 1.0f));
    
    // 澄明な光沢ハイライトを乗算加算し、潤った透明水滴としての艶をもたせる
    sceneColor.rgb += float3(totalGlint, totalGlint, totalGlint);
    
    output.color = sceneColor;
    output.color.a = 1.0f;
    
    return output;
}
