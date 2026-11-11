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

    FMatrix MakeRootMotionMatrix(const FVector& Position, const FQuat& Rotation, const FVector& Scale)
    {
        const FMatrix RotationMatrix = Rotation.ToFMatrix();

        return FMatrix(
            RotationMatrix[0][0] * Scale.X, RotationMatrix[0][1] * Scale.X, RotationMatrix[0][2] * Scale.X, 0.0f,
            RotationMatrix[1][0] * Scale.Y, RotationMatrix[1][1] * Scale.Y, RotationMatrix[1][2] * Scale.Y, 0.0f,
            RotationMatrix[2][0] * Scale.Z, RotationMatrix[2][1] * Scale.Z, RotationMatrix[2][2] * Scale.Z, 0.0f,
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

    const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
    for (const FModelBone& Bone : Skeleton->GetBones())
    {
        if (Bone.ParentBoneIndex == -1)
        {
            const int32 BoneNodeIndex = Bone.NodeIndex;
            if (Nodes.IsValidIndex(BoneNodeIndex))
            {
                const int32 ParentNodeIndex = Nodes[BoneNodeIndex].ParentNodeIndex;
                if (Nodes.IsValidIndex(ParentNodeIndex) &&
                    _stricmp(Nodes[ParentNodeIndex].Name.c_str(), "root") == 0)
                {
                    return ParentNodeIndex;
                }
            }
            return BoneNodeIndex;
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

    if (!DataModel || !std::isfinite(StartTime) || !std::isfinite(DeltaTime) || DataModel->PlayLength <= 0.0f)
    {
        return false;
    }

    const float PlayLength = DataModel->PlayLength;
    const int32 RootNodeIndex = GetRootBoneNodeIndex();

    if (!Skeleton || RootNodeIndex < 0)
    {
        return false;
    }

    const TArray<FModelNode>& Nodes = Skeleton->GetNodes();

    if (!Nodes.IsValidIndex(RootNodeIndex))
    {
        return false;
    }

    TArray<int32> RootNodeChain;
    int32 CurrentNodeIndex = RootNodeIndex;

    while (CurrentNodeIndex != -1)
    {
        if (!Nodes.IsValidIndex(CurrentNodeIndex))
        {
            return false;
        }

        RootNodeChain.Add(CurrentNodeIndex);

        if (RootNodeChain.Num() > Nodes.Num())
        {
            return false;
        }

        CurrentNodeIndex = Nodes[CurrentNodeIndex].ParentNodeIndex;
    }

    auto GetRootComponentMatrix = [&](float Time, FMatrix& OutMatrix) -> bool
        {
            FMatrix GlobalMatrix = FMatrix::Identity;

            for (int32 ChainIndex = RootNodeChain.Num() - 1; ChainIndex >= 0; --ChainIndex)
            {
                const int32 NodeIndex = RootNodeChain[ChainIndex];
                FMatrix LocalMatrix = Nodes[NodeIndex].LocalTransform;
                bool bFoundTrack = false;

                for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
                {
                    if (Track.NodeIndex != NodeIndex)
                    {
                        continue;
                    }

                    FVector Position;
                    FVector Scale;
                    FQuat Rotation;

                    if (!SampleAnimationTrack(Track.InternalTrack, DataModel, Time, Position, Rotation, Scale))
                    {
                        return false;
                    }

                    LocalMatrix = MakeRootMotionMatrix(Position, Rotation, Scale);
                    bFoundTrack = true;
                    break;
                }

                if (NodeIndex == RootNodeIndex && !bFoundTrack)
                {
                    return false;
                }

                GlobalMatrix = LocalMatrix * GlobalMatrix;
            }

            OutMatrix = GlobalMatrix;
            return true;
        };

    FMatrix RootStartMatrix;

    if (!GetRootComponentMatrix(0.0f, RootStartMatrix))
    {
        return false;
    }

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

            FMatrix FromMatrix;
            FMatrix ToMatrix;

            if (!GetRootComponentMatrix(FromTime, FromMatrix) || !GetRootComponentMatrix(ToTime, ToMatrix))
            {
                return false;
            }

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

static FString NormalizeBoneName(const FString& InName)
{
    FString Out;
    Out.reserve(InName.size());
    for (char c : InName)
    {
        Out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    size_t UnderscorePos = 0;
    while ((UnderscorePos = Out.find('_', UnderscorePos)) != FString::npos)
    {
        size_t DigitStart = UnderscorePos + 1;
        if (DigitStart < Out.size() && Out[DigitStart] == '0')
        {
            size_t NonZero = Out.find_first_not_of('0', DigitStart);
            if (NonZero != FString::npos && std::isdigit(static_cast<unsigned char>(Out[NonZero])))
            {
                Out.erase(DigitStart, NonZero - DigitStart);
            }
        }
        UnderscorePos++;
    }
    return Out;
}

void UAnimSequenceBase::RetargetToSkeleton(USkeleton* InSkeleton)
{
    if (!InSkeleton || !DataModel)
    {
        return;
    }

    SetSkeleton(InSkeleton);

    const TArray<FModelNode>& TargetNodes = InSkeleton->GetNodes();
    if (TargetNodes.IsEmpty())
    {
        return;
    }

    TMap<FString, int32> NameToNodeMap;
    TMap<FString, int32> NormNameToNodeMap;
    for (int32 i = 0; i < TargetNodes.Num(); ++i)
    {
        NameToNodeMap.Add(TargetNodes[i].Name, i);
        NormNameToNodeMap.Add(NormalizeBoneName(TargetNodes[i].Name), i);
    }

    for (FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
    {
        const FString RawName = Track.Name.ToString();
        const int32* FoundIndex = NameToNodeMap.Find(RawName);
        if (!FoundIndex)
        {
            FoundIndex = NormNameToNodeMap.Find(NormalizeBoneName(RawName));
        }

        if (FoundIndex)
        {
            Track.NodeIndex = *FoundIndex;

            // 스케일 불일치 보정 (예: 순수 애니메이션 FBX에서 root 스케일이 0.01인데 스켈레톤의 root 스케일은 1.0인 경우)
            if (!Track.InternalTrack.ScaleKeys.IsEmpty())
            {
                const FMatrix& NodeTransform = TargetNodes[*FoundIndex].LocalTransform;
                const float NodeScaleX = FVector(NodeTransform.M[0][0], NodeTransform.M[0][1], NodeTransform.M[0][2]).Size();
                const float TrackScaleX = Track.InternalTrack.ScaleKeys[0].X;

                if (TrackScaleX > 0.0f && NodeScaleX > 0.0f)
                {
                    const float Ratio = NodeScaleX / TrackScaleX;
                    if (Ratio > 50.0f && Ratio < 150.0f)
                    {
                        for (FVector& SKey : Track.InternalTrack.ScaleKeys)
                        {
                            SKey *= Ratio;
                        }
                        for (FVector& PKey : Track.InternalTrack.PosKeys)
                        {
                            PKey *= Ratio;
                        }
                    }
                }
            }
        }
        else
        {
            if (Track.NodeIndex >= 0 && Track.NodeIndex < TargetNodes.Num() &&
                NormalizeBoneName(TargetNodes[Track.NodeIndex].Name) == NormalizeBoneName(RawName))
            {
                // 기존 인덱스가 이미 일치하는 노드를 가리키고 있음
            }
            else
            {
                Track.NodeIndex = -1;
            }
        }
    }
}
