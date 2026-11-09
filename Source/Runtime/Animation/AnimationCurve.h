#pragma once

#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"

enum class EAnimationCurveInterpolation : uint8
{
	Constant,
	Linear,
	Cubic
};

struct FAnimationCurveKey
{
	float Time = 0.0f;
	float Value = 0.0f;
	float ArriveTangent = 0.0f;
	float LeaveTangent = 0.0f;
	EAnimationCurveInterpolation Interpolation = EAnimationCurveInterpolation::Linear;
};

struct FAnimationCurve
{
	FName Name;
	FName LayerName;
	int32 NodeIndex = -1;
	TArray<FAnimationCurveKey> Keys;
};

struct FAnimationCurveData
{
	TArray<FAnimationCurve> Curves;
};