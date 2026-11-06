#pragma once

#include "Animation/AnimNode_Base.h"

struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A;
	FPoseLink B;
	float Alpha = 0.0f;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
};