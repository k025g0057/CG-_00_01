// --- Object3D.PS.hlsl ---

#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingModel; // 0: Lambertian, 1: Half-Lambert
    float32_t2 padding; // パディングを合わせる
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color; //!< ライトの色
    float32_t3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Material> gMaterial : register(b0);

// textureの受け取り
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting != 0)
    {
        // 1. 法線とライトの逆向きの内積（NdotL）を計算
        float32_t3 N = normalize(input.normal);
        float32_t3 L = -gDirectionalLight.direction;
        float32_t NdotL = dot(N, L);

        float32_t cos = 0.0f;

        // 2. ライティングモデルの切り替え
        if (gMaterial.lightingModel == 0)
        {
            // Lambertian Reflectance (ランバート反射)
            cos = saturate(NdotL);
        }
        else if (gMaterial.lightingModel == 1)
        {
            // Half-Lambert (ハーフランバート)
            cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        }

        // 光の計算結果を色に適用
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        // Lightingしない場合
        output.color = gMaterial.color * textureColor;
    }

    return output;
}