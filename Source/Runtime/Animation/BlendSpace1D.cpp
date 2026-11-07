#include "EnginePCH.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"

#include <algorithm>
#include <cmath>

bool UBlendSpace1D::AddSample(float SampleValue, UAnimSequence* Animation)
{
    if (!Animation || !std::isfinite(SampleValue))
    {
        return false;
    }
    for (FBlendSample1D& Sample : Samples)
    {
        if (std::abs(Sample.SampleValue - SampleValue) < 1.e-4f)
        {
            Sample.Animation = Animation;
            return true;
        }
    }
    Samples.Add({ SampleValue, Animation });
    std::sort(Samples.begin(), Samples.end(), [](const FBlendSample1D& A, const FBlendSample1D& B) {return A.SampleValue < B.SampleValue;});
    return true;
}

bool UBlendSpace1D::GetBlendSamples(float Input, int32& OutIndexA, int32 OutIndexB, float& OutAlpha) const
{
    if (Samples.IsEmpty())
    {
        return false;
    }
    if (!std::isfinite(Input))
    {
        return false;
    }
    const int32 LastIndex = Samples.Num() - 1;
    if (Input <= Samples[0].SampleValue)
    {
        OutIndexA = 0;
        OutIndexB = 0;
        OutAlpha = 0.0f;
        return true;
    }

    if (Input >= Samples[LastIndex].SampleValue)
    {
        OutIndexA = LastIndex;
        OutIndexB = LastIndex;
        OutAlpha = 0.0f;
        return true;
    }
    for (int32 Index = 0; Index < LastIndex; ++Index)
    {
        const float ValueA = Samples[Index].SampleValue;
        const float ValueB = Samples[Index + 1].SampleValue;
        if (Input >= ValueA && Input <= ValueB)
        {
            OutIndexA = Index;
            OutIndexB = Index + 1;
            OutAlpha = (Input - ValueA) / (ValueB - ValueA);
            OutAlpha = std::clamp(OutAlpha, 0.0f, 1.0f);
            return true;
        }
    }
    return false;
}
