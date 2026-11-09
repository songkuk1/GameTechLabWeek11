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
}

void FAnimNode_SequencePlayer::SetPlaybackPosition(float InTime)
{
    const float PlayLength = GetPlayLength();
    if (PlayLength <= 0.0f || !std::isfinite(InTime))
    {
        CurrentTime = 0.0f;
        return;
    }
    CurrentTime = std::clamp(InTime, 0.0f, PlayLength);
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

    if (Context.AnimInstance && std::isfinite(Context.NotifyWeight) && Context.NotifyWeight > 0.0001f)
    {
        Context.AnimInstance->QueueAnimNotifyUpdate(this, Sequence, PreviousTime, ActualAdvance, CurrentTime, PlayLength, bLoop);
    }
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

        const float FramePosition = std::clamp(CurrentTime * FrameRate, 0.0f, static_cast<float>(KeyCount - 1));

        const int32 FrameA = static_cast<int32>(std::floor(FramePosition));
        const int32 FrameB = std::min(FrameA + 1, KeyCount - 1);
        const float Alpha = FramePosition - FrameA;
        const FVector Position = RawTrack.PosKeys[FrameA] + (RawTrack.PosKeys[FrameB] - RawTrack.PosKeys[FrameA]) * Alpha;
        const FQuat Rotation = FQuat::Slerp(RawTrack.RotKeys[FrameA], RawTrack.RotKeys[FrameB], Alpha);
        const FVector Scale = RawTrack.ScaleKeys[FrameA] + (RawTrack.ScaleKeys[FrameB] - RawTrack.ScaleKeys[FrameA]) * Alpha;

        Output.Pose[NodeIndex] = MakeAnimationMatrix(Position, Rotation, Scale);
    }
}
