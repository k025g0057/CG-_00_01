// --- 追加：C++側から送られてくる行列データ（b1）を受け取る構造体 ---
struct TransformationMatrix
{
    float32_t4x4 wvp;
};

// register(b1) で、C++側の「1番目のスロット」と紐付けます
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // ★★★ 頂点の座標に行列を掛け算します
    // mul(座標, 行列) という関数を使って計算します
    output.position = mul(input.position, gTransformationMatrix.wvp);
    
    return output;
}