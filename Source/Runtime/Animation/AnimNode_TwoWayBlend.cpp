#include "EnginePCH.h"
#include "AnimNode_TwoWayBlend.h"
#include "Animation/AnimationRuntime.h"

void FAnimNode_TwoWayBlend::SetSourceNodes(FAnimNode_Base* InSourceA, FAnimNode_Base* InSourceB)
{
	SourcePoseA = InSourceA;
	SourcePoseB = InSourceB;
}

void FAnimNode_TwoWayBlend::SetAlpha(float InAlpha)
{
	Alpha = std::isfinite(InAlpha) ? std::clamp(InAlpha, 0.0f, 1.0f) : 0.0f;
}

void FAnimNode_TwoWayBlend::Update(const FAnimationUpdateContext& Context)
{
    FAnimationUpdateContext ContextA = Context;
    ContextA.NotifyWeight *= (1.0f - Alpha);

    FAnimationUpdateContext ContextB = Context;
    ContextB.NotifyWeight *= Alpha;

    ContextA.BlendWeight *= (1.0f - Alpha);
    ContextB.BlendWeight *= Alpha;

	if (SourcePoseA)
	{
		SourcePoseA->Update(ContextA);
	}
	if (SourcePoseB && SourcePoseB != SourcePoseA)
	{
		SourcePoseB->Update(ContextB);
	}
}

void FAnimNode_TwoWayBlend::Evaluate(FPoseContext& Output)
{
    if (!SourcePoseA && !SourcePoseB)
    {
        return;
    }

    if (!SourcePoseA)
    {
        SourcePoseB->Evaluate(Output);
        return;
    }

    if (!SourcePoseB)
    {
        SourcePoseA->Evaluate(Output);
        return;
    }

    if (Alpha <= 0.0f)
    {
        SourcePoseA->Evaluate(Output);
        return;
    }

    if (Alpha >= 1.0f)
    {
        SourcePoseB->Evaluate(Output);
        return;
    }

    FPoseContext PoseA;
    FPoseContext PoseB;

    PoseA.Pose.SetNum(Output.Pose.Num());
    PoseB.Pose.SetNum(Output.Pose.Num());

    for (int32 Index = 0; Index < Output.Pose.Num(); ++Index)
    {
        PoseA.Pose[Index] = Output.Pose[Index];
        PoseB.Pose[Index] = Output.Pose[Index];
    }

    SourcePoseA->Evaluate(PoseA);
    SourcePoseB->Evaluate(PoseB);

    AnimationRuntime::BlendPoses(PoseA, PoseB, Alpha, Output);
}

bool FAnimNode_TwoWayBlend::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
    if (!SourcePoseA)
    {
        return SourcePoseB && SourcePoseB->EvaluateCurve(CurveName, LayerName, OutValue);
    }

    if (!SourcePoseB)
    {
        return SourcePoseA->EvaluateCurve(CurveName, LayerName, OutValue);
    }

    float ValueA = 0.0f;
    float ValueB = 0.0f;

    const bool bHasCurveA = SourcePoseA->EvaluateCurve(CurveName, LayerName, ValueA);
    const bool bHasCurveB = SourcePoseB->EvaluateCurve(CurveName, LayerName, ValueB);

    if (!bHasCurveA && !bHasCurveB)
    {
        return false;
    }

    OutValue = ValueA * (1.0f - Alpha) + ValueB * Alpha;
    return std::isfinite(OutValue);
}
FAnimNode_TwoWayBlendPlayer::FAnimNode_TwoWayBlendPlayer()
{
    BlendNode.SetSourceNodes(&SequencePlayerA, &SequencePlayerB);
}

void FAnimNode_TwoWayBlendPlayer::SetAnimations(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB)
{
    SequencePlayerA.SetSequence(InSequenceA);
    SequencePlayerB.SetSequence(InSequenceB);

    SetPlayRate(PlayRate);
}

void FAnimNode_TwoWayBlendPlayer::SetAlpha(float InAlpha)
{
    BlendNode.SetAlpha(InAlpha);
}

float FAnimNode_TwoWayBlendPlayer::GetAlpha() const
{
    return BlendNode.GetAlpha();
}

void FAnimNode_TwoWayBlendPlayer::SetPlaybackPosition(float InTime)
{
    const float LengthA = SequencePlayerA.GetPlayLength();
    const float LengthB = SequencePlayerB.GetPlayLength();

    if (LengthA <= 0.0f || LengthB <= 0.0f || !std::isfinite(InTime))
    {
        return;
    }

    const float Progress = std::clamp(InTime / LengthA, 0.0f, 1.0f);

    SequencePlayerA.SetPlaybackPosition(Progress * LengthA);

    SequencePlayerB.SetPlaybackPosition(Progress * LengthB);
}

float FAnimNode_TwoWayBlendPlayer::GetCurrentTime() const
{
    return SequencePlayerA.GetCurrentTime();
}

float FAnimNode_TwoWayBlendPlayer::GetPlayLength() const
{
    return SequencePlayerA.GetPlayLength();
}

float FAnimNode_TwoWayBlendPlayer::GetFrameRate() const
{
    return SequencePlayerA.GetFrameRate();
}

void FAnimNode_TwoWayBlendPlayer::SetPlayRate(float InPlayRate)
{
    PlayRate = std::isfinite(InPlayRate) ? InPlayRate : 1.0f;

    const float LengthA = SequencePlayerA.GetPlayLength();
    const float LengthB = SequencePlayerB.GetPlayLength();

    SequencePlayerA.SetPlayRate(PlayRate);

    if (LengthA > 0.0f && LengthB > 0.0f)
    {
        SequencePlayerB.SetPlayRate(PlayRate * (LengthB / LengthA));
    }
    else
    {
        SequencePlayerB.SetPlayRate(PlayRate);
    }
}

void FAnimNode_TwoWayBlendPlayer::SetLooping(bool bInLoop)
{
    SequencePlayerA.SetLooping(bInLoop);
    SequencePlayerB.SetLooping(bInLoop);
}

bool FAnimNode_TwoWayBlendPlayer::IsLooping() const
{
    return SequencePlayerA.IsLooping();
}

void FAnimNode_TwoWayBlendPlayer::Update(const FAnimationUpdateContext& Context)
{
    BlendNode.Update(Context);
}

void FAnimNode_TwoWayBlendPlayer::Evaluate(FPoseContext& Output)
{
    BlendNode.Evaluate(Output);
}

bool FAnimNode_TwoWayBlendPlayer::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
    return BlendNode.EvaluateCurve(CurveName, LayerName, OutValue);
}