#pragma once
#undef GetCurrentTime
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Core/NameTypes.h"

class UAnimInstance;

class IAnimPlaybackControl
{
public:
    virtual ~IAnimPlaybackControl() = default;
    virtual void SetPlaybackPosition(float Time) = 0;
    virtual float GetCurrentTime() const = 0;
    virtual float GetPlayLength() const = 0;
    virtual float GetFrameRate() const = 0;
    virtual void SetPlayRate(float Rate) = 0;
    virtual float GetPlayRate() const = 0;
    virtual void SetLooping(bool bLoop) = 0;
    virtual bool IsLooping() const = 0;
};

struct FAnimationUpdateContext
{
    float DeltaTime = 0.0f;
    UAnimInstance* AnimInstance = nullptr;
    float NotifyWeight = 1.0f;
    float BlendWeight = 1.0f;
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
    virtual bool EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const { return false; }
    virtual IAnimPlaybackControl* GetPlaybackControl() { return nullptr; }
    virtual const IAnimPlaybackControl* GetPlaybackControl() const { return nullptr; }
};