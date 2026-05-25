#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

//textureの受け取り
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // ★1つ目のコードを追加：テクスチャの色をサンプリング
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // ★2つ目のコードを追加：マテリアルの色とテクスチャの色を掛け合わせる
    output.color = gMaterial.color * textureColor;

    return output;
}