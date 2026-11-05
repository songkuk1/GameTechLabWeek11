#pragma pack_matrix(row_major)

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

cbuffer QuadParams : register(b1)
{
    matrix World;
    float4 Color;
    int HasTexture;
    int3 QuadParamsPadding;
};

Texture2D Texture : register(t0);

SamplerState Sampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mul(float4(input.position, 1.0f), World), VP);
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 FinalColor = Color;
    if (HasTexture)
    {
        FinalColor = Texture.Sample(Sampler, input.uv) * Color;
    }
    
    return FinalColor;
}
