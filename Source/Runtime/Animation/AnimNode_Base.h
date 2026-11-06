#pragma once

#include "Containers/Array.h"
#include "Math/Matrix.h"

struct FAnimationUpdateContext
{
	float DeltaTime = 0.0f;
};

struct FPoseContext
{
	TArray<FMatrix> Pose;
};

struct FAnimNode_Base
{
	virtual ~FAnimNode_Base() = default;
	virtual void Update(const FAnimationUpdateContext& Context) {}
	virtual void Evaluate(FPoseContext& Output) {}
};