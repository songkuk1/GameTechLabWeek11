#ifndef HITORI_COLOR_BLEND_INCLUDED
#define HITORI_COLOR_BLEND_INCLUDED

// EColorBlendMode와 일치하는 색상 합성 상수
static const uint COLOR_BLEND_ALPHA = 0u;
static const uint COLOR_BLEND_MULTIPLY = 1u;

// 적용 비중이 RGB에 이미 반영된 색상의 합성
float3 BlendPremultipliedColor(float3 Base, float3 WeightedLayer, float Coverage, uint BlendMode)
{
    const float Weight = saturate(Coverage);

    if (BlendMode == COLOR_BLEND_MULTIPLY)
    {
        return Base * ((1.0f - Weight) + WeightedLayer);
    }

    return Base * (1.0f - Weight) + WeightedLayer;
}

// 일반 색상과 적용 비중을 사용하는 색상 합성
float3 BlendColor(float3 Base, float3 Layer, float Opacity, uint BlendMode)
{
    const float Weight = saturate(Opacity);
    return BlendPremultipliedColor(Base, Layer * Weight, Weight, BlendMode);
}

#endif