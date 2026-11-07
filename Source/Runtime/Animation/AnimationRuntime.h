#pragma once

#include "Animation/AnimNode_Base.h"

namespace AnimationRuntime
{
	void BlendPoses(const FPoseContext& PoseA, const FPoseContext& PoseB, float Alpha, FPoseContext& OutPose);
}