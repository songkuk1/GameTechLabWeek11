#include "EnginePCH.h"
#include "AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include <algorithm>
#include <cmath>

namespace
{
	FMatrix MakeAnimationMatrix(const FVector& Position, const FQuat& Rotation, const FVector& Scale)
	{
		const FMatrix RotationMatrix = Rotation.ToFMatrix();
        return FMatrix(
            RotationMatrix[0][0] * Scale.X, RotationMatrix[0][1] * Scale.X, RotationMatrix[0][2] * Scale.X, 0.0f,
            RotationMatrix[1][0] * Scale.Y, RotationMatrix[1][1] * Scale.Y, RotationMatrix[1][2] * Scale.Y, 0.0f,
            RotationMatrix[2][0] * Scale.Z, RotationMatrix[2][1] * Scale.Z, RotationMatrix[2][2] * Scale.Z, 0.0f,
            Position.X,                     Position.Y,                     Position.Z,                     1.0f
        );
	}
}

void FAnimNode_SequencePlayer::SetSequence(UAnimSequence* InSequence)
{
    Sequence = InSequence;
    CurrentTime = 0.0f;
    bHasUpdated = false;
    bExtractRootMotion = false;
    CurrentRootMotionScale = 1.0f;
}

void FAnimNode_SequencePlayer::SetPlaybackPosition(float InTime)
{
    const float PlayLength = GetPlayLength();

    if (PlayLength <= 0.0f || !std::isfinite(InTime))
    {
        CurrentTime = 0.0f;
        bHasUpdated = false;
        return;
    }

    CurrentTime = std::clamp(InTime, 0.0f, PlayLength);
    bHasUpdated = false;
}

float FAnimNode_SequencePlayer::GetPlayLength() const
{
    if (!Sequence || !Sequence->GetDataModel())
    {
        return 0.0f;
    }
    const float PlayLength = Sequence->GetDataModel()->PlayLength;
    if (!std::isfinite(PlayLength) || PlayLength <= 0.0f)
    {
        return 0.0f;
    }
    return PlayLength;
}

float FAnimNode_SequencePlayer::GetFrameRate() const
{
    if (!Sequence || !Sequence->GetDataModel())
    {
        return 0.0f;
    }
    const float FrameRate = Sequence->GetDataModel()->FrameRate.AsDecimal();
    if (!std::isfinite(FrameRate) || FrameRate <= 0.0f)
    {
        return 0.0f;
    }
    return FrameRate;
}

void FAnimNode_SequencePlayer::SetPlayRate(float InPlayRate)
{
    PlayRate = std::isfinite(InPlayRate) ? InPlayRate : 1.0f;
}

void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
    bExtractRootMotion = bAllowRootMotion && Context.AnimInstance && Context.AnimInstance->IsRootMotionEnabled();
    CurrentRootMotionScale = (bAllowRootMotion && Context.AnimInstance) ? Context.AnimInstance->GetRootMotionScale() : 1.0f;
    if (!Sequence || !Sequence->GetDataModel())
    {
        return;
    }

    const float PlayLength = GetPlayLength();

    if (PlayLength <= 0.0f)
    {
        CurrentTime = 0.0f;
        return;
    }

    const float PreviousTime = CurrentTime;
    float ActualAdvance = 0.0f;

    if (std::isfinite(Context.DeltaTime) && Context.DeltaTime > 0.0f && std::isfinite(PlayRate) && PlayRate != 0.0f)
    {
        const float RequestedAdvance = Context.DeltaTime * PlayRate;

        if (std::isfinite(RequestedAdvance))
        {
            const float UnwrappedTime = CurrentTime + RequestedAdvance;

            if (bLoop)
            {
                CurrentTime = std::fmod(UnwrappedTime, PlayLength);

                if (CurrentTime < 0.0f)
                {
                    CurrentTime += PlayLength;
                }

                ActualAdvance = RequestedAdvance;
            }
            else
            {
                CurrentTime = std::clamp(UnwrappedTime, 0.0f, PlayLength);
                ActualAdvance = CurrentTime - PreviousTime;
            }
        }
    }

    if (bExtractRootMotion && Context.AnimInstance && Context.BlendWeight > 0.0001f)
    {
        FVector RootMotionTranslation;
        FQuat RootMotionRotation;

        Sequence->ExtractRootMotion(PreviousTime, ActualAdvance, bLoop, RootMotionTranslation, RootMotionRotation);
        Context.AnimInstance->QueueRootMotionDelta(RootMotionTranslation, RootMotionRotation, Context.BlendWeight);
    }

    const bool bIsFirstUpdate = !bHasUpdated;

    if (Context.AnimInstance && std::isfinite(Context.NotifyWeight) && Context.NotifyWeight > 0.0001f)
    {
        Context.AnimInstance->QueueAnimNotifyUpdate(this, Sequence, PreviousTime, ActualAdvance, CurrentTime, PlayLength, bLoop, bIsFirstUpdate);
    }

    bHasUpdated = true;
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
    if (!Sequence || !Sequence->GetDataModel())
    {
        return;
    }
    const UAnimDataModel* DataModel = Sequence->GetDataModel();
    const float FrameRate = GetFrameRate();
    if (FrameRate <= 0.0f)
    {
        return;
    }
    const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();

    for (const FBoneAnimationTrack& Track : Tracks)
    {
        const int32 NodeIndex = Track.NodeIndex;
        if (!Output.Pose.IsValidIndex(NodeIndex))
        {
            continue;
        }
        const FRawAnimSequenceTrack& RawTrack = Track.InternalTrack;
        const int32 KeyCount = RawTrack.PosKeys.Num();
        if (KeyCount <= 0 || RawTrack.RotKeys.Num() != KeyCount || RawTrack.ScaleKeys.Num() != KeyCount)
        {
            continue;
        }

        int32 FrameA = 0;
        int32 FrameB = 0;
        float Alpha = 0.0f;

        const int32 LastFullFrameIndex = static_cast<int32>(std::floor(DataModel->PlayLength * FrameRate));
        const float LastFullFrameTime = static_cast<float>(LastFullFrameIndex) / FrameRate;
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
        const FVector Position = RawTrack.PosKeys[FrameA] + (RawTrack.PosKeys[FrameB] - RawTrack.PosKeys[FrameA]) * Alpha;
        const FQuat Rotation = FQuat::Slerp(RawTrack.RotKeys[FrameA], RawTrack.RotKeys[FrameB], Alpha);
        const FVector Scale = RawTrack.ScaleKeys[FrameA] + (RawTrack.ScaleKeys[FrameB] - RawTrack.ScaleKeys[FrameA]) * Alpha;

        Output.Pose[NodeIndex] = MakeAnimationMatrix(Position, Rotation, Scale);
    }

    const int32 RootNodeIndex = Sequence->GetRootBoneNodeIndex();

    if (Output.Pose.IsValidIndex(RootNodeIndex))
    {
        FVector RootPosition;
        FVector RootScale;
        FQuat RootRotation;

        if (Sequence->GetRootBoneTransform(0.0f, RootPosition, RootRotation, RootScale))
        {
            if (bExtractRootMotion)
            {
                Output.Pose[RootNodeIndex] = MakeAnimationMatrix(RootPosition, RootRotation, RootScale);
            }
            else if (std::abs(CurrentRootMotionScale - 1.0f) > 1.0e-4f)
            {
                FMatrix& RootPose = Output.Pose[RootNodeIndex];
                const FVector CurrentPos(RootPose[3][0], RootPose[3][1], RootPose[3][2]);
                const FVector ScaledPos = RootPosition + (CurrentPos - RootPosition) * CurrentRootMotionScale;
                RootPose[3][0] = ScaledPos.X;
                RootPose[3][1] = ScaledPos.Y;
                RootPose[3][2] = ScaledPos.Z;
            }
        }
    }
}

bool FAnimNode_SequencePlayer::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
    if (!Sequence || CurveName.IsNone() || !std::isfinite(CurrentTime))
    {
        return false;
    }

    const UAnimDataModel* DataModel = Sequence->GetDataModel();
    if (!DataModel)
    {
        return false;
    }

    const FAnimationCurve* Curve = DataModel->FindCurve(CurveName, LayerName);
    if (!Curve || Curve->Keys.IsEmpty())
    {
        return false;
    }

    const float Value = Curve->Evaluate(CurrentTime);
    if (!std::isfinite(Value))
    {
        return false;
    }

    OutValue = Value;
    return true;
}