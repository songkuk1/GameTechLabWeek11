#include "EnginePCH.h"
#include "AnimationRuntime.h"
#include <algorithm>
#include <cassert>
void FAnimationRuntime::BlendTwoPosesTogether(const TArray<FAnimTransform>& PoseA, const TArray<FAnimTransform>& PoseB, float WeightA, TArray<FAnimTransform>& OutPose)
{
	if (PoseA.Num() != PoseB.Num())
	{
		assert(false && "Cannot blend poses with different bone counts.");
		return;
	}
	WeightA = std::clamp(WeightA, 0.0f, 1.0f);
	const float WeightB = 1.0f - WeightA;
	OutPose.SetNum(PoseA.Num());

	for (int32 Index = 0; Index < PoseA.Num(); ++Index)
	{
		const FAnimTransform& A = PoseA[Index];
		const FAnimTransform& B = PoseB[Index];
		FAnimTransform& Result = OutPose[Index];

		Result.Position = A.Position * WeightA + B.Position * WeightB;
		Result.Rotation = FQuat::Slerp(A.Rotation, B.Rotation, WeightB);
		Result.Scale = A.Scale * WeightA + B.Scale * WeightB;
	}
}
