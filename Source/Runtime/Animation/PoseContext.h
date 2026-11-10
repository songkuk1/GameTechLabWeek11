#pragma once

#include "Containers/Array.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Transform.h"

struct PoseTransform
{
	FVector Translation;
	FQuat Rotation;
	FVector Scale;
};

struct PoseContext
{
	const TArray<PoseTransform>& ReferencePose;
	TArray<PoseTransform> LocalPoseTrans;
};

struct AnimationContext
{
	TArray<FString>& TriggeredNotifyNames;
	FTransform& RootTransform;
	float deltaTime = 0.0f;
	float Speed = 0.0f;
	bool& bIsAttacking;
	bool& bIsHit;
	bool& bIsJumpStart;
	bool& bIsJumpAir;
	bool& bIsJumpEnd;
	int32& RootMotionNodeIndex;
};
