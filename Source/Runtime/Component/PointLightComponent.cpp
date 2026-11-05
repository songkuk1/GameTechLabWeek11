#include "EnginePCH.h"
#include "PointLightComponent.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightInfo.h"
#include "Render/LineBatcher.h"
#include "Math/EngineMath.h"

void UPointLightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	// 음수 반경은 감쇠 계산을 뒤집으므로 막는다.
	if (AttenuationRadius < 0.0f)
		AttenuationRadius = 0.0f;

	if (Intensity < 0.0f)
		Intensity = 0.0f;
}

FPointLightInfo UPointLightComponent::GetPointLightInfo() const
{
	FPointLightInfo Info;
	Info.Color = GetShaderColorWithIntensity();
	Info.AttenuationRadius = AttenuationRadius;
	Info.Position = GetWorldLocation();
	return Info;
}


void UPointLightComponent::GetShadowInfo(TArray<FShadowInfo>& OutShadowInfo) const
{
	//FOV는 90도
	//ViewProjection이 달라서 월드 축 6방향으로 찍는다.
	OutShadowInfo.Reset();

	float FOV = 93.6f;

	FShadowInfo ShadowInfo;
	ShadowInfo.Bias = GetShadowBias();
	ShadowInfo.Near = NearClipPlane;
	ShadowInfo.Far = AttenuationRadius;
	ShadowInfo.LightSize = LightSourceRadius;
	ShadowInfo.ProjScale = 1.f / (2.f * std::tan(FMath::DegreesToRadians(FOV) * 0.5f));
	ShadowInfo.AtlasSize = ShadowResolution;

	//FRotator Pitch Yaw Roll 순서
	//Forward 0.f, 0.f, 0.f
	FRotator Rotator = FRotator(0.f, 0.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, FOV, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Back 0.f, 180.f, 0.f
	Rotator = FRotator(0.f, 180.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, FOV, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Right 0.f, 90.f, 0.f
	Rotator = FRotator(0.f, 90.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, FOV, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Left 0.f, -90.f, 0.f
	Rotator = FRotator(0.f, -90.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, FOV, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Up -90.f, 0.f, 0.f
	Rotator = FRotator(-90.f, 0.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, FOV, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Down 90.f, 0.f, 0.f
	Rotator = FRotator(90.f, 0.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, FOV, 1.f);
	OutShadowInfo.Add(ShadowInfo);
}


FLightSceneProxy* UPointLightComponent::CreateSceneProxy()
{
	return new FPointLightSceneProxy(this);
}

void UPointLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (!LineBatcher) return;

	constexpr int32 CircleSegmentCount = 32;
	const FVector Center = GetWorldLocation();

	// 디버그 라인은 오버레이라 화면에 보이는 sRGB 값을 감마 변환 없이 그대로 쓴다
	const FLinearColor LineColor = LightColor.ReinterpretAsLinear();
	const FVector4 Color(LineColor.R, LineColor.G, LineColor.B, 1.0f);

	// XY·XZ·YZ 평면의 원 세 개로 감쇠 반경 구를 나타낸다.
	const FVector PlaneAxes[3][2] =
	{
		{ FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f) },
		{ FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) },
		{ FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) },
	};
	for (const auto& Axes : PlaneAxes)
	{
		FVector PreviousPoint = Center + Axes[0] * AttenuationRadius;
		for (int32 i = 1; i <= CircleSegmentCount; ++i)
		{
			const float Angle = (2.0f * PI * i) / CircleSegmentCount;
			const FVector Point = Center
				+ Axes[0] * (std::cos(Angle) * AttenuationRadius)
				+ Axes[1] * (std::sin(Angle) * AttenuationRadius);
			LineBatcher->AddLine(PreviousPoint, Point, Color);
			PreviousPoint = Point;
		}
	}
}
