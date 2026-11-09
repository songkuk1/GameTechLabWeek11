#pragma once

#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"
#include <algorithm>
#include <cmath>

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

    float Evaluate(float Time) const
    {
        if (Keys.IsEmpty() || !std::isfinite(Time))
        {
            return 0.0f;
        }

        if (Time <= Keys[0].Time)
        {
            return Keys[0].Value;
        }

        for (int32 Index = 0; Index < Keys.Num() - 1; ++Index)
        {
            const FAnimationCurveKey& KeyA = Keys[Index];
            const FAnimationCurveKey& KeyB = Keys[Index + 1];

            if (Time >= KeyB.Time)
            {
                continue;
            }

            const float Duration = KeyB.Time - KeyA.Time;

            if (Duration <= 1.0e-6f)
            {
                return KeyB.Value;
            }

            const float Alpha = std::clamp((Time - KeyA.Time) / Duration, 0.0f, 1.0f);

            switch (KeyA.Interpolation)
            {
            case EAnimationCurveInterpolation::Constant:
                return KeyA.Value;

            case EAnimationCurveInterpolation::Linear:
                return KeyA.Value + (KeyB.Value - KeyA.Value) * Alpha;

            case EAnimationCurveInterpolation::Cubic:
            {
                const float AlphaSquared = Alpha * Alpha;
                const float AlphaCubed = AlphaSquared * Alpha;

                const float H00 = 2.0f * AlphaCubed - 3.0f * AlphaSquared + 1.0f;
                const float H10 = AlphaCubed - 2.0f * AlphaSquared + Alpha;
                const float H01 = -2.0f * AlphaCubed + 3.0f * AlphaSquared;
                const float H11 = AlphaCubed - AlphaSquared;

                return H00 * KeyA.Value + H10 * Duration * KeyA.LeaveTangent + H01 * KeyB.Value + H11 * Duration * KeyB.ArriveTangent;
            }
            }
        }

        return Keys.Last().Value;
    }
};

struct FAnimationCurveData
{
	TArray<FAnimationCurve> Curves;
};