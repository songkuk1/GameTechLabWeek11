#include "EnginePCH.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include <algorithm>
#include <cmath>

namespace
{
    bool SampleAnimationTrack(const FRawAnimSequenceTrack& Track, const UAnimDataModel* DataModel, float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale)
    {
        if (!DataModel || !std::isfinite(Time) || DataModel->PlayLength <= 0.0f)
        {
            return false;
        }
        const float FrameRate = DataModel->FrameRate.AsDecimal();
        const int32 KeyCount = Track.PosKeys.Num();

        if (!std::isfinite(FrameRate) || FrameRate <= 0.0f || KeyCount <= 0 || Track.RotKeys.Num() != KeyCount || Track.ScaleKeys.Num() != KeyCount)
        {
            return false;
        }

        const float CurrentTime = std::clamp(Time, 0.0f, DataModel->PlayLength);
        const int32 LastFullFrameIndex = static_cast<int32>(std::floor(DataModel->PlayLength * FrameRate));
        const float LastFullFrameTime = static_cast<float>(LastFullFrameIndex) / FrameRate;

        int32 FrameA = 0;
        int32 FrameB = 0;
        float Alpha = 0.0f;

        const bool bHasPartialLastFrame = LastFullFrameIndex < KeyCount - 1 && DataModel->PlayLength - LastFullFrameTime > 1.0e-6f;

        if (bHasPartialLastFrame && CurrentTime > LastFullFrameTime)
        {
            FrameA = LastFullFrameIndex;
            FrameB = std::min(FrameA + 1, KeyCount - 1);
            Alpha = std::clamp((CurrentTime - LastFullFrameTime) / (DataModel->PlayLength - LastFullFrameTime), 0.0f, 1.0f);
        }
        else
        {
            const float FramePosition = std::clamp(CurrentTime * FrameRate, 0.0f, static_cast<float>(KeyCount - 1));
            FrameA = static_cast<int32>(std::floor(FramePosition));
            FrameB = std::min(FrameA + 1, KeyCount - 1);
            Alpha = FramePosition - FrameA;
        }
        OutPosition = Track.PosKeys[FrameA] + (Track.PosKeys[FrameB] - Track.PosKeys[FrameA]) * Alpha;
        OutRotation = FQuat::Slerp(Track.RotKeys[FrameA], Track.RotKeys[FrameB], Alpha);
        OutScale = Track.ScaleKeys[FrameA] + (Track.ScaleKeys[FrameB] - Track.ScaleKeys[FrameA]) * Alpha;
        return true;
    }

    FMatrix MakeRootMotionMatrix(const FVector& Position, const FQuat& Rotation)
    {
        const FMatrix RotationMatrix = Rotation.ToFMatrix();

        return FMatrix(
            RotationMatrix[0][0], RotationMatrix[0][1], RotationMatrix[0][2], 0.0f,
            RotationMatrix[1][0], RotationMatrix[1][1], RotationMatrix[1][2], 0.0f,
            RotationMatrix[2][0], RotationMatrix[2][1], RotationMatrix[2][2], 0.0f,
            Position.X, Position.Y, Position.Z, 1.0f
        );
    }
}

UAnimSequenceBase::UAnimSequenceBase()
{
	DataModel = CreateDefaultSubobject<UAnimDataModel>(FName("DataModel"));
}

void UAnimSequenceBase::SerializeNotifies(FStructuredArchive::FRecord Record)
{
    FArchive& Archive = Record.GetUnderlyingArchive();
    const bool bIsLoading = Archive.IsLoading();

    if (bIsLoading)
    {
        Notifies.Reset();
    }

    int32 NotifyCount = bIsLoading ? 0 : Notifies.Num();
    FStructuredArchive::FArray NotifyArray = Record.EnterArray("Notifies", NotifyCount);

    for (int32 Index = 0; Index < NotifyCount; ++Index)
    {
        FStructuredArchive::FRecord NotifyRecord = NotifyArray.EnterElement().EnterRecord();

        float TriggerTime = bIsLoading ? 0.0f : Notifies[Index].TriggerTime;
        float Duration = bIsLoading ? 0.0f : Notifies[Index].Duration;
        FName NotifyName = bIsLoading ? NAME_None : Notifies[Index].NotifyName;

        NotifyRecord << SA_VALUE("TriggerTime", TriggerTime);
        NotifyRecord << SA_VALUE("Duration", Duration);
        NotifyRecord << SA_VALUE("NotifyName", NotifyName);

        if (!bIsLoading)
        {
            continue;
        }

        const float PlayLength = DataModel ? DataModel->PlayLength : 0.0f;

        if (!std::isfinite(TriggerTime) || !std::isfinite(Duration) || TriggerTime < 0.0f || Duration < 0.0f || NotifyName.IsNone() || PlayLength <= 0.0f || TriggerTime > PlayLength || TriggerTime + Duration > PlayLength)
        {
            HTR_LOG(Warning, "AnimSequence: invalid notify at index {}", Index);
            continue;
        }

        AddNotify(TriggerTime, Duration, NotifyName);
    }
}

int32 UAnimSequenceBase::GetRootBoneNodeIndex() const
{
    if (!Skeleton)
    {
        return -1;
    }

    for (const FModelBone& Bone : Skeleton->GetBones())
    {
        if (Bone.ParentBoneIndex == -1)
        {
            return Bone.NodeIndex;
        }
    }
    return -1;
}

bool UAnimSequenceBase::GetRootBoneTransform(float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const
{
    if (!DataModel)
    {
        return false;
    }

    const int32 RootNodeIndex = GetRootBoneNodeIndex();

    if (RootNodeIndex < 0)
    {
        return false;
    }

    for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
    {
        if (Track.NodeIndex == RootNodeIndex)
        {
            return SampleAnimationTrack(Track.InternalTrack, DataModel, Time, OutPosition, OutRotation, OutScale);
        }
    }

    return false;
}

bool UAnimSequenceBase::ExtractRootMotion(float StartTime, float DeltaTime, bool bLooping, FVector& OutTranslation, FQuat& OutRotation) const
{
    OutTranslation = FVector::ZeroVector;
    OutRotation = FQuat::Identity();

    if (DataModel || !std::isfinite(StartTime) || !std::isfinite(DeltaTime) || DataModel->PlayLength <= 0.0f)
    {
        return false;
    }

    FVector RootStartPosition;
    FVector RootStartScale;
    FQuat RootStartRotation;

    if (!GetRootBoneTransform(0.0f, RootStartPosition, RootStartRotation, RootStartScale))
    {
        return false;
    }

    const float PlayLength = DataModel->PlayLength;
    const FMatrix RootStartMatrix = MakeRootMotionMatrix(RootStartPosition, RootStartRotation);
    const FMatrix RootStartInverse = RootStartMatrix.Inverse();

    FMatrix AccumulatedDelta = FMatrix::Identity;

    auto AddSegment = [&](float FromTime, float ToTime) -> bool
        {
            FVector FromPosition;
            FVector FromScale;
            FQuat FromRotation;
            FVector ToPosition;
            FVector ToScale;
            FQuat ToRotation;

            if (!GetRootBoneTransform(FromTime, FromPosition, FromRotation, FromScale) || !GetRootBoneTransform(ToTime, ToPosition, ToRotation, ToScale))
            {
                return false;
            }

            const FMatrix FromMatrix = MakeRootMotionMatrix(FromPosition, FromRotation);
            const FMatrix ToMatrix = MakeRootMotionMatrix(ToPosition, ToRotation);

            const FMatrix FromRelative = RootStartInverse * FromMatrix;
            const FMatrix ToRelative = RootStartInverse * ToMatrix;
            const FMatrix SegmentDelta = ToRelative * FromRelative.Inverse();
            AccumulatedDelta = SegmentDelta * AccumulatedDelta;
            return true;
        };

    if (!bLooping)
    {
        const float FromTime = std::clamp(StartTime, 0.0f, PlayLength);
        const float ToTime = std::clamp(StartTime + DeltaTime, 0.0f, PlayLength);

        if (!AddSegment(FromTime, ToTime))
        {
            return false;
        }
    }
    else
    {
        float CurrentTime = std::fmod(StartTime, PlayLength);
        if (CurrentTime < 0.0f)
        {
            CurrentTime += PlayLength;
        }

        if (DeltaTime < 0.0f && CurrentTime <= 1.0e-6f && StartTime > 0.0f)
        {
            CurrentTime = PlayLength;
        }

        float Remaining = DeltaTime;
        int32 IterationCount = 0;

        while (std::abs(Remaining) > 1.0e-6f && IterationCount++ < 4096)
        {
            if (Remaining > 0.0f)
            {
                if (CurrentTime >= PlayLength - 1.0e-6f)
                {
                    CurrentTime = 0.0f;
                }

                const float Segment = std::min(Remaining, PlayLength - CurrentTime);

                if (Segment <= 1.0e-6f)
                {
                    CurrentTime = 0.0f;
                    continue;
                }

                if (!AddSegment(CurrentTime, CurrentTime + Segment))
                {
                    return false;
                }

                CurrentTime += Segment;
                Remaining -= Segment;

                if (CurrentTime >= PlayLength - 1.0e-6f)
                {
                    CurrentTime = 0.0f;
                }
            }
            else
            {
                if (CurrentTime <= 1.0e-6f)
                {
                    CurrentTime = PlayLength;
                }

                const float Segment = std::min(-Remaining, CurrentTime);

                if (Segment <= 1.0e-6f)
                {
                    CurrentTime = PlayLength;
                    continue;
                }

                if (!AddSegment(CurrentTime, CurrentTime - Segment))
                {
                    return false;
                }

                CurrentTime -= Segment;
                Remaining += Segment;

                if (CurrentTime <= 1.0e-6f)
                {
                    CurrentTime = PlayLength;
                }
            }
        }

        if (std::abs(Remaining) > 1.0e-6f)
        {
            return false;
        }
    }

    OutTranslation = FVector(AccumulatedDelta[3][0], AccumulatedDelta[3][1], AccumulatedDelta[3][2]);
    OutRotation = MatrixToRotator(AccumulatedDelta).Quaternion().Normalized();

    return true;
}
