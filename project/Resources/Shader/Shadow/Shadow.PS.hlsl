struct VSOutput {
    float4 position : SV_POSITION;
};

void main(VSOutput input) {
    // 深度値は自動的に書き込まれるため、ピクセルシェーダーは空でよい
    // (disableRTV = true なのでレンダーターゲットへの出力はない)
}
