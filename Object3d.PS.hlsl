#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
};

struct DirectionalLight
{
    float32_t4 color; //!< ライトの色
    float32_t3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
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

    // 1. テクスチャの色をサンプリング（ここはそのまま）
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // ＝★ 【スライドの通り変更】ライティング計算の追加 ＝
    if (gMaterial.enableLighting != 0)
    { // Lightingする場合
        // 法線と「光の逆向き（-gDirectionalLight.direction）」の内積（当たっている角度）を求め、[0, 1]に収める
        float32_t cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        
        // テクスチャ、マテリアル、ライトの色、当たり具合（cos）、明るさをすべて掛け合わせる
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    { // Lightingしない場合（Spriteなど。前回までと同じ演算）
        output.color = gMaterial.color * textureColor;
    }

    return output;
}