#pragma once
#include "Math/Color.h"

struct FMinimalViewInfo
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation;
	float FOV = 60.0f;
	float AspectRatio = 1.777778f; // 16:9
	bool bConstrainAspectRatio = false;
	float OrthoWidth = 16.0f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	bool bOrthographic = false;

	FMatrix CalculateViewMatrix() const;
	FMatrix CalculateProjectionMatrix(float Aspect) const;
	FMatrix CalculateViewProjectionMatrix(float Aspect) const;

	FRay DeProjection(const FVector2& MousePos, float ScreenWidth, float ScreenHeight) const;

	void BlendViewInfo(const FMinimalViewInfo& OtherInfo, float OtherWeight);
};

struct FBillboardView
{
	FVector Location;
	FVector Forward;
	FVector Right;
	FVector Up;

	static FBillboardView FromView(const FMinimalViewInfo& View);

	FMatrix MakeMatrix(const FVector& Position, float Width, float Height) const;
};

// 화면 가장자리를 어둡게 하는 효과. 셰이더에서 톤매핑 전 Linear 색에 곱한다
struct FVignetteSettings
{
	bool bEnabled = false;
	float Intensity = 0.9f;
	float Radius = 0.4f;
	float Softness = 0.35f;
};

// 카메라가 화면 전체에 거는 효과의 최종값. 렌더러는 이 값만 읽는다
struct FCameraScreenEffects
{
	FLinearColor FadeColor = FLinearColor::Black;
	float FadeAmount = 0.f;

	FVignetteSettings Vignette;
};
