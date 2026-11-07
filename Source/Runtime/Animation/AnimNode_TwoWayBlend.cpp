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
	if (SourcePoseA)
	{
		SourcePoseA->Update(Context);
	}
	if (SourcePoseB && SourcePoseB != SourcePoseA)
	{
		SourcePoseB->Update(Context);
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
