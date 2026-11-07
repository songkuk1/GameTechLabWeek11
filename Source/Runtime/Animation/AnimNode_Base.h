#pragma once

#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"

class UAnimInstance;

struct FAnimationUpdateContext
{
    float DeltaTime = 0.0f;
    UAnimInstance* AnimInstance = nullptr;
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