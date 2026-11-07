#include "EnginePCH.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Render/SkeletalMesh.h"
#include <algorithm>
#include <cmath>

bool UBlendSpace1D::SetAxis(const FBlendSpaceAxis1D& InAxis)
{
    if (InAxis.Name.IsNone() || !std::isfinite(InAxis.MinValue) || !std::isfinite(InAxis.MaxValue) || InAxis.MinValue >= InAxis.MaxValue || InAxis.GridDivisions < 1 || InAxis.GridDivisions > 64)
    {
        return false;
    }

    for (const FBlendSample1D& Sample : Samples)
    {
        if (!std::isfinite(Sample.SampleValue) || Sample.SampleValue < InAxis.MinValue || Sample.SampleValue > InAxis.MaxValue)
        {
            return false;
        }
    }

    Axis = InAxis;
    return true;
}

bool UBlendSpace1D::AddSample(float SampleValue, UAnimSequence* Animation, float RateScale)
{
    if (!Skeleton || !Animation ||  Animation->GetSkeleton() != Skeleton || !std::isfinite(SampleValue) || !std::isfinite(RateScale) || RateScale <= 0.0f || SampleValue < Axis.MinValue || SampleValue > Axis.MaxValue)
    {
        return false;
    }

    constexpr float Tolerance = 1.e-4f;
    for (const FBlendSample1D& Sample : Samples)
    {
        if (std::abs(Sample.SampleValue - SampleValue) < Tolerance)
        {
            return false;
        }
    }
    FBlendSample1D NewSample;
    NewSample.SampleValue = SampleValue;
    NewSample.Animation = Animation;
    NewSample.RateScale = RateScale;
    Samples.Add(std::move(NewSample));
    std::sort(Samples.begin(), Samples.end(), [](const FBlendSample1D& A, const FBlendSample1D& B){ return A.SampleValue < B.SampleValue; });

    return true;
}

bool UBlendSpace1D::RemoveSample(int32 SampleIndex)
{
    if (!Samples.IsValidIndex(SampleIndex))
    {
        return false;
    }

    Samples.RemoveAt(SampleIndex);
    return true;
}

bool UBlendSpace1D::SetSampleValue(int32 SampleIndex, float SampleValue)
{
    if (!Samples.IsValidIndex(SampleIndex) || !std::isfinite(SampleValue) || SampleValue < Axis.MinValue || SampleValue > Axis.MaxValue)
    {
        return false;
    }
    constexpr float Tolerance = 1.e-4f;
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        if (Index == SampleIndex)
        {
            continue;
        }
        if (std::abs(Samples[Index].SampleValue - SampleValue) < Tolerance)
        {
            return false;
        }
    }
    Samples[SampleIndex].SampleValue = SampleValue;
    std::sort(Samples.begin(), Samples.end(), [](const FBlendSample1D& A, const FBlendSample1D& B){ return A.SampleValue < B.SampleValue; });

    return true;
}

bool UBlendSpace1D::ReplaceSampleAnimation(int32 SampleIndex, UAnimSequence* Animation)
{
    if (!Samples.IsValidIndex(SampleIndex) || !Animation || !Skeleton || Animation->GetSkeleton() != Skeleton)
    {
        return false;
    }
    Samples[SampleIndex].Animation = Animation;
    return true;
}

bool UBlendSpace1D::SetSampleRateScale(int32 SampleIndex, float RateScale)
{
    if (!Samples.IsValidIndex(SampleIndex) || !std::isfinite(RateScale) || RateScale <= 0.0f)
    {
        return false;
    }
    Samples[SampleIndex].RateScale = RateScale;
    return true;
}

bool UBlendSpace1D::GetBlendSamples(float Input, int32& OutIndexA, int32& OutIndexB, float& OutAlpha) const
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

bool UBlendSpace1D::SetSkeleton(USkeleton* InSkeleton)
{
    if (!InSkeleton)
    {
        return false;
    }
    for (const FBlendSample1D& Sample : Samples)
    {
        if (!Sample.Animation || Sample.Animation->GetSkeleton() != InSkeleton)
        {
            return false;
        }
    }
    Skeleton = InSkeleton;
    return true;
}

bool UBlendSpace1D::SetPreviewMesh(USkeletalMesh* InPreviewMesh)
{
    if (!Skeleton || !InPreviewMesh || InPreviewMesh->GetSkeleton() != Skeleton)
    {
        return false;
    }

    PreviewMesh = InPreviewMesh;
    return true;
}

bool UBlendSpace1D::IsBlendSpaceDataValid() const
{
    if (!bSerializationValid || !Skeleton)
    {
        return false;
    }

    if (Axis.Name.IsNone() || !std::isfinite(Axis.MinValue) || !std::isfinite(Axis.MaxValue) || Axis.MinValue >= Axis.MaxValue || Axis.GridDivisions < 1 || Axis.GridDivisions > 64)
    {
        return false;
    }

    if (PreviewMesh && PreviewMesh->GetSkeleton() != Skeleton)
    {
        return false;
    }

    for (const FBlendSample1D& Sample : Samples)
    {
        if (!Sample.Animation || Sample.Animation->GetSkeleton() != Skeleton || !std::isfinite(Sample.SampleValue) || !std::isfinite(Sample.RateScale) || Sample.RateScale <= 0.0f || Sample.SampleValue < Axis.MinValue || Sample.SampleValue > Axis.MaxValue)
        {
            return false;
        }
    }

    return true;
}

void UBlendSpace1D::Serialize(FStructuredArchive::FRecord Record)
{
    Super::Serialize(Record);
    FArchive& Archive = Record.GetUnderlyingArchive();
    const bool bIsLoading = Archive.IsLoading();
    if (bIsLoading)
    {
        bSerializationValid = true;
        Samples.Empty();
        PreviewMesh = nullptr;
    }
    FName AxisName = Axis.Name;
    float MinValue = Axis.MinValue;
    float MaxValue = Axis.MaxValue;
    int32 GridDivisions = Axis.GridDivisions;
    Record << SA_VALUE("AxisName", AxisName);
    Record << SA_VALUE("MinValue", MinValue);
    Record << SA_VALUE("MaxValue", MaxValue);
    Record << SA_VALUE("GridDivisions", GridDivisions);

    if (bIsLoading)
    {
        FBlendSpaceAxis1D LoadedAxis;
        LoadedAxis.Name = AxisName;
        LoadedAxis.MinValue = MinValue;
        LoadedAxis.MaxValue = MaxValue;
        LoadedAxis.GridDivisions = GridDivisions;
        if (!SetAxis(LoadedAxis))
        {
            bSerializationValid = false;
            HTR_LOG(Warning, "BlendSpace: invalid axis data");
        }
    }
    UObject* SkeletonObject = Skeleton;
    Record << SA_VALUE("Skeleton", SkeletonObject);
    if (bIsLoading)
    {
        Skeleton = Cast<USkeleton>(SkeletonObject);

        if (!Skeleton)
        {
            bSerializationValid = false;
        }
    }
    UObject* PreviewMeshObject = PreviewMesh;
    Record << SA_VALUE("PreviewMesh", PreviewMeshObject);
    if (bIsLoading)
    {
        PreviewMesh = Cast<USkeletalMesh>(PreviewMeshObject);
        if (PreviewMeshObject && !PreviewMesh)
        {
            bSerializationValid = false;
        }
        if (PreviewMesh && PreviewMesh->GetSkeleton() != Skeleton)
        {
            PreviewMesh = nullptr;
            bSerializationValid = false;
        }
    }

    int32 SampleCount = Samples.Num();
    FStructuredArchive::FArray SampleArray = Record.EnterArray("Samples", SampleCount);
    if (bIsLoading)
    {
        Samples.Empty();
    }
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        FStructuredArchive::FRecord SampleRecord = SampleArray.EnterElement().EnterRecord();
        float SampleValue = bIsLoading ? 0.0f : Samples[Index].SampleValue;
        float RateScale = bIsLoading ? 1.0f : Samples[Index].RateScale;
        UObject* AnimationObject = bIsLoading ? nullptr : Samples[Index].Animation;
        SampleRecord << SA_VALUE("SampleValue", SampleValue);
        SampleRecord << SA_VALUE("RateScale", RateScale);
        SampleRecord << SA_VALUE("Animation", AnimationObject);
        if (bIsLoading)
        {
            UAnimSequence* Animation = Cast<UAnimSequence>(AnimationObject);
            if (!AddSample(SampleValue, Animation, RateScale))
            {
                bSerializationValid = false;
                HTR_LOG(Warning, "BlendSpace: invalid sample at index {}", Index);
            }
        }
    }
}
