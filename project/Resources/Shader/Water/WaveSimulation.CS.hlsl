// WaveSimulation.CS.hlsl
// 2D波動方程式を用いた波の伝播シミュレーション
// 速度成分にのみ減衰をかけることで、波が通過した後に水面が元の高さに戻る

cbuffer SimulationParams : register(b0)
{
    float gAlpha;        // 波の伝播速度係数 (c^2 * dt^2 / dx^2)
    float gDamping;      // 減衰係数 (速度にのみ適用, 例: 0.95 ～ 0.99)
    int   gSourceCount;  // 現在の波源の数
    float gResetFlag;    // 1.0 なら状態をゼロにリセットするフラグ
    
    // x, y = UV座標 (0.0 ～ 1.0)
    // z = 波源の半径 (UV空間での半径)
    // w = 波源の強さ (高さの加算値)
    float4 gSources[16]; 
};

// t0: 1フレーム前のハイトマップ
Texture2D<float> gPrevHeight : register(t0);
// t1: 2フレーム前のハイトマップ
Texture2D<float> gPrevPrevHeight : register(t1);

// u0: 出力先のハイトマップ
RWTexture2D<float> gOutHeight : register(u0);

[numthreads(16, 16, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint width, height;
    gOutHeight.GetDimensions(width, height);

    if (DTid.x >= width || DTid.y >= height)
        return;

    int3 pos = int3(DTid.x, DTid.y, 0);

    // 境界チェック付きで前回のサンプリング
    float h1 = gPrevHeight.Load(pos);
    float h2 = gPrevPrevHeight.Load(pos);

    // リセットフラグが立っている場合は強制的に0クリア
    if (gResetFlag > 0.5f)
    {
        gOutHeight[DTid.xy] = 0.0f;
        return;
    }

    // 上下左右のピクセル値を取得（境界では自分自身をクランプ扱い）
    int xLeft  = max((int)DTid.x - 1, 0);
    int xRight = min((int)DTid.x + 1, (int)width - 1);
    int yUp    = max((int)DTid.y - 1, 0);
    int yDown  = min((int)DTid.y + 1, (int)height - 1);

    float hLeft  = gPrevHeight.Load(int3(xLeft, DTid.y, 0));
    float hRight = gPrevHeight.Load(int3(xRight, DTid.y, 0));
    float hUp    = gPrevHeight.Load(int3(DTid.x, yUp, 0));
    float hDown  = gPrevHeight.Load(int3(DTid.x, yDown, 0));

    // 波動方程式（速度ベースの減衰）
    // velocity = h1 - h2 (前フレームとの差が速度に相当)
    // laplacian = 空間的な広がり
    float laplacian = hLeft + hRight + hUp + hDown - 4.0f * h1;
    
    // 速度にのみ減衰をかける方式:
    // h_new = h1 + damping * (h1 - h2) + alpha * laplacian
    // これにより波のエネルギーは減衰するが、
    // 静止時 (h1 == h2 == 0, laplacian == 0) → h_new = 0 に正しく収束する
    float velocity = (h1 - h2) * gDamping;
    float hNew = h1 + velocity + gAlpha * laplacian;

    // 波源（力）の加算
    float2 uv = float2((float)DTid.x / (float)(width - 1), (float)DTid.y / (float)(height - 1));
    for (int i = 0; i < gSourceCount; ++i)
    {
        float2 sourceUV = gSources[i].xy;
        float radius = gSources[i].z;
        float strength = gSources[i].w;

        float dist = distance(uv, sourceUV);
        if (dist < radius)
        {
            // スムーズな減衰関数
            float t = dist / radius;
            float falloff = (1.0f - t * t);
            falloff *= falloff; // (1 - t²)² — なだらかに減衰
            hNew += strength * falloff;
        }
    }

    // 発散（NaN/Infinity）を防ぐためのクランプ
    hNew = clamp(hNew, -0.5f, 0.5f);

    gOutHeight[DTid.xy] = hNew;
}
