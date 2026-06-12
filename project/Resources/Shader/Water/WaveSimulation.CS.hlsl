// WaveSimulation.CS.hlsl
// 2D波動方程式を用いた波の伝播シミュレーション

cbuffer SimulationParams : register(b0)
{
    float gAlpha;        // 波の伝播速度係数 (c^2 * dt^2 / dx^2)
    float gDamping;      // 減衰係数 (例: 0.98 ～ 0.995)
    int   gSourceCount;  // 現在の波源の数
    float gPadding;
    
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

    // 上下左右のピクセル値を取得（境界では自分自身をミラーリング・クランプ扱い）
    int xLeft  = max((int)DTid.x - 1, 0);
    int xRight = min((int)DTid.x + 1, (int)width - 1);
    int yUp    = max((int)DTid.y - 1, 0);
    int yDown  = min((int)DTid.y + 1, (int)height - 1);

    float hLeft  = gPrevHeight.Load(int3(xLeft, DTid.y, 0));
    float hRight = gPrevHeight.Load(int3(xRight, DTid.y, 0));
    float hUp    = gPrevHeight.Load(int3(DTid.x, yUp, 0));
    float hDown  = gPrevHeight.Load(int3(DTid.x, yDown, 0));

    // 波動方程式の差分スキーム
    // h_new = 2 * h1 - h2 + alpha * (hLeft + hRight + hUp + hDown - 4 * h1)
    float laplacian = hLeft + hRight + hUp + hDown - 4.0f * h1;
    float hNew = (2.0f * h1 - h2 + gAlpha * laplacian) * gDamping;

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
            // なだらかに力を加える (1 - (dist/radius)^2)^2 などのガウス関数的な減衰
            float falloff = 1.0f - (dist / radius);
            hNew += strength * falloff * falloff;
        }
    }

    gOutHeight[DTid.xy] = hNew;
}
