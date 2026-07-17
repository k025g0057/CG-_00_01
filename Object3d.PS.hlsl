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
        // 1. 法線とライトの逆向きの内積（NdotL）を求める（ここではsaturateせず [-1, 1] のまま）
        float32_t NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);

        // 2. [-1, 1] の範囲を [0, 1] に変換して2乗する： (NdotL * 0.5 + 0.5)^2
        float32_t cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // テクスチャ、マテリアル、ライトの色、当たり具合（cos）、明るさをすべて掛け合わせる
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    { // Lightingしない場合（Spriteなど。前回までと同じ演算）
        output.color = gMaterial.color * textureColor;
    }

    return output;
}