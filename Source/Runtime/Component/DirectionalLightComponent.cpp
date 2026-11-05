#include "EnginePCH.h"
#include "DirectionalLightComponent.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LineBatcher.h"

FDirectionalLightInfo UDirectionalLightComponent::GetDirectionalLightInfo() const
{
	FDirectionalLightInfo Info;
	Info.Color = GetShaderColorWithIntensity();
	Info.Direction = GetLightDirection();
	return Info;
}

FShadowInfo UDirectionalLightComponent::GetShadowInfo() const
{
    FShadowInfo ShadowInfo;
    // OrthoWidth, FarClipPlane(예: 10000.0f) 적용
    const float FarClip = 10000.0f;
    const float AspectRatio = 1.0f; // 섀도우 맵은 2048x2048 정사각형이므로 1.0

    // View 행렬 (라이트 위치와 방향 기준)
    FMatrix View = GetViewMatrix();

    // Orthographic Projection 행렬
    // (OrthoWidth를 가로/세로 범위로 직교 박스 생성)
    const float Width = OrthoWidth;
    const float Height = Width / AspectRatio;
    const float NearClip = 0.1f;

    const float XScale = 2.0f / Width;
    const float YScale = 2.0f / Height;
    const float ZScale = 1.0f / (FarClip - NearClip);
    const float ZOffset = -NearClip / (FarClip - NearClip);
    FMatrix Proj = FMatrix(
        0.0f, 0.0f, ZScale, 0.0f,
        XScale, 0.0f, 0.0f, 0.0f,
        0.0f, YScale, 0.0f, 0.0f,
        0.0f, 0.0f, ZOffset, 1.0f
    );
    ShadowInfo.LightViewProjection = View * Proj;
    ShadowInfo.Bias = GetShadowBias();
    ShadowInfo.NormalBias = ShadowNormalBias;
    ShadowInfo.Near = NearClip;
    ShadowInfo.Far = FarClip;
    ShadowInfo.AtlasSize = ShadowResolution;
    return ShadowInfo;
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy()
{
	return new FDirectionalLightSceneProxy(this);
}

void UDirectionalLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
    if (!LineBatcher) return;

    const FVector Center = GetWorldLocation();
    const FVector Forward = GetLightDirection().GetSafeNormal();

    // Forward가 위/아래(Z축)를 똑바로 바라보고 있으면 Y축을 기준으로, 아니면 Z축을 기준으로 삼음
    const FVector UpGuide = (std::abs(Forward.Z) > 0.99f) ? FVector(0.0f, 1.0f, 0.0f) : FVector(0.0f, 0.0f, 1.0f);

    const FVector Right = FVector::Cross(Forward, UpGuide).GetSafeNormal();
    const FVector Up = FVector::Cross(Right, Forward).GetSafeNormal();

    const FLinearColor ColorLinear = LightColor.ReinterpretAsLinear();
    const FVector4 Color = FVector4(ColorLinear.R, ColorLinear.G, ColorLinear.B, 1.0f);
    const float Length = 3.0f;
    const float thickness = 0.1f;
    const float Spacing = 0.5f;

    LineBatcher->AddArrow(Center, Forward, Length, thickness, Spacing, Color);
}
