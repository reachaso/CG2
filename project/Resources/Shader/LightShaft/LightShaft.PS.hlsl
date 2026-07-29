#include "../Fullscreen/Fullscreen.hlsli"
#include "../Caustics/Caustics.hlsli"

// =============================================================================
// LightShaft ポストエフェクト（水中の降り注ぐ光・光柱）
//
// カメラから各ピクセルのシーン位置へ向かってレイマーチし、水面より下の区間で
// 「その位置の真上の水面が明るいか」を積算する。水面の明るさには Caustics と
// 同じ Voronoi パターンを使うので、光柱の断面と床に落ちる網目が一致する。
//
//   1. 深度からシーンのワールド座標を復元し、カメラからのレイを作る
//   2. レイと水面（y = waterHeight）の交差を解いて、水中の区間 [t0, t1] だけに
//      マーチ範囲を絞る（無駄なサンプルを打たない）
//   3. 各サンプルで CausticsPatternCheap を評価し、水面からの深さで指数減衰
//   4. 平均値 × 光路長に分解し、固定の基準長 kShaftReferenceLength で
//      正規化して加算合成（maxDistance は明るさに影響しない）
//
// 遮蔽:
//   レイの終点をシーン深度で打ち切っているため、手前のオブジェクトが光柱を
//   自然に遮る。空（深度=遠クリップ）のピクセルは maxDistance まで伸ばす。
//
// 前提:
//   - t1 は深度SRV (DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
//   - 行列は row-vector 規約（DXC に -Zpr が渡されているため mul(vector, matrix)）
//
// コスト:
//   1ピクセルあたり最大 sampleCount 回 Voronoi を評価する（実際のステップ数は
//   水中区間の長さに比例して減る）。ここが唯一の重い部分なので、
//   FPS が落ちる場合はまず Sample Count を下げること。
// =============================================================================

// t0: シーンカラー
Texture2D<float32_t4> gTexture : register(t0);
// t1: 深度バッファ
Texture2D<float32_t> gDepthTexture : register(t1);

SamplerState gSamplerLinear : register(s0); // linear wrap
SamplerState gSamplerPoint : register(s1);  // point wrap

// b1: LightShaft パラメータ
cbuffer LightShaftParams : register(b1) {
    float4x4 projectionInverse; // プロジェクション逆行列
    float4x4 viewInverse;       // ビュー逆行列（= カメラのワールド行列）
    float4 shaftColor;          // 光柱の色（RGB / A は未使用）
    float time;                 // 経過時間
    float intensity;            // 全体の強さ
    float scale;                // 模様の密度（Caustics と揃えること）
    float speed;                // うねる速度（Caustics と揃えること）
    float contrast;             // 断面のコントラスト
    float waterHeight;          // 水面のワールドY座標
    float density;              // 深さによる指数減衰の強さ
    float maxDistance;          // レイマーチの最大距離
    int sampleCount;            // レイマーチのステップ数（性能の主要因）
    float ditherStrength;       // バンディング対策のディザ量 (0 ~ 1)
    float lerpFactor;           // 水中ブレンド率 (0:適用しない ~ 1:完全適用)
    float _padding;
};

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

// 明るさ正規化とステップ数適応の基準となる光路長（ワールド単位）。
// この長さだけ水中を通ったレイで光量が飽和する。
// maxDistance とは独立にしてあるので、maxDistance を伸ばしても暗くならない。
static const float kShaftReferenceLength = 60.0f;

// -----------------------------------------------------------------------------
// 深度からビュー空間座標を復元する
// -----------------------------------------------------------------------------
float3 ReconstructViewPos(float2 uv, float depth) {
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 viewPos = mul(ndc, projectionInverse);
    return viewPos.xyz / viewPos.w;
}

PixelShaderOutPut main(VertexShaderOutput input) {
    PixelShaderOutPut output;

    float2 uv = input.texcoord;
    float4 baseColor = gTexture.Sample(gSamplerLinear, uv);
    output.color = float4(baseColor.rgb, 1.0f);

    // 画面全体で一様な分岐なのでコストは無い
    if (lerpFactor <= 0.0f || intensity <= 0.0f || sampleCount <= 0) {
        return output;
    }

    // --- レイの構築 ---
    float depthVal = gDepthTexture.Sample(gSamplerPoint, uv);
    float3 viewPos = ReconstructViewPos(uv, depthVal);
    float3 scenePos = mul(float4(viewPos, 1.0f), viewInverse).xyz;

    // カメラのワールド座標（row-vector 規約なので原点を viewInverse で変換）
    float3 camPos = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), viewInverse).xyz;

    float3 toScene = scenePos - camPos;
    float sceneDist = length(toScene);

    // 完全に退化したレイ（カメラと同一点）は捨てる
    if (sceneDist < 1e-4f) {
        return output;
    }
    float3 rayDir = toScene / sceneDist;

    // 空（遠クリップ面）でも maxDistance までは光柱を出す。
    // オブジェクトがあればそこでレイを打ち切る＝遮蔽になる。
    float rayEnd = min(sceneDist, maxDistance);

    // --- レイと水面の交差から、水中の区間 [t0, t1] を求める ---
    // P(t) = camPos + rayDir * t、条件は P(t).y < waterHeight
    float t0 = 0.0f;
    float t1 = rayEnd;

    if (abs(rayDir.y) < 1e-5f) {
        // ほぼ水平なレイ：カメラが水面より上なら区間は空
        if (camPos.y >= waterHeight) {
            return output;
        }
    } else {
        float tPlane = (waterHeight - camPos.y) / rayDir.y;
        if (rayDir.y < 0.0f) {
            // 下を向いている：水面を跨いだ後が水中
            t0 = max(t0, tPlane);
        } else {
            // 上を向いている：水面に達する前が水中
            t1 = min(t1, tPlane);
        }
    }

    if (t1 <= t0) {
        return output;
    }

    // --- ステップ数の適応 ---
    // 区間が短いのに常に sampleCount 回まわすと、水面すぐ下を見ただけで
    // フル価格を払うことになる。区間長に比例させて無駄を削る。
    float segment = t1 - t0;
    // 最低 4 ステップは確保し、上限は Sample Count に収める。
    int steps = (int)(float(sampleCount) * saturate(segment / kShaftReferenceLength)) + 4;
    steps = min(steps, max(sampleCount, 4));

    float stepLen = segment / float(steps);

    // ピクセルごとに開始位置をずらしてステップの縞（バンディング）を消す。
    // R2 低食い違い量列を使うと市松模様にならず均一にばらける。
    float dither = frac(dot(input.position.xy, float2(0.7548776662f, 0.5698402909f)));
    float startOffset = lerp(0.5f, dither, saturate(ditherStrength));

    float accum = 0.0f;
    for (int i = 0; i < steps; ++i) {
        float t = t0 + (float(i) + startOffset) * stepLen;
        float3 samplePos = camPos + rayDir * t;

        // 区間を絞っているので depthBelow は常に正（数値誤差分だけ max で保護）
        float depthBelow = max(waterHeight - samplePos.y, 0.0f);

        // 真上の水面の明るさ。Caustics と同じ座標系・同じ位相なので
        // 床に落ちる網目と光柱の位置が一致する。
        float surfaceLight = CausticsPatternCheap(samplePos.xz * scale, time, speed, contrast);

        // 深いところほど光が届かない
        accum += surfaceLight * exp(-density * depthBelow);
    }

    // 平均値 × 光路長 に分解して正規化する。
    // maxDistance で割ると「マーチ距離を伸ばすと暗くなる」という直感に反する
    // 挙動になるので、固定の基準長で正規化して maxDistance は純粋に
    // 「どこまでマーチするか（＝コスト）」だけを制御させる。
    float mean = accum / float(steps);
    float pathFactor = saturate(segment / kShaftReferenceLength);

    // 網目は細い線なので mean は小さな値になる。
    // intensity を 0~2 程度の直感的な範囲で扱えるように経験的な係数を掛ける。
    accum = mean * pathFactor * 12.0f;

    // --- 加算合成 ---
    float3 finalColor = baseColor.rgb + shaftColor.rgb * accum * intensity * lerpFactor;

    output.color = float4(finalColor, 1.0f);
    return output;
}
