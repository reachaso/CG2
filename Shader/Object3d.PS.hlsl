struct PixelShaderOutput {
    float4 color : SV_TARGET0; // 出力する色
};

PixelShaderOutput main()
{
    PixelShaderOutput output;
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f); // 色の設定
    return output;
}
