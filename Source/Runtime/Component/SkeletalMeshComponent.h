#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"

class USkeleton;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    virtual ~USkeletalMeshComponent() override;

    void PlayAnimation(UAnimSequence* InSequence);

    void PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha);
    void SetAnimationBlendAlpha(float InAlpha);
    float GetAnimationBlendAlpha() const;

    void TickAnimation(float DeltaSeconds);

    virtual const FModelMeshData* GetMeshData() const override;

    void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

    FPrimitiveSceneProxy* CreateSceneProxy() override;

    UAnimInstance* GetAnimInstance() const { return AnimInstance; }
    void SetAnimInstanceClass(UClass* InAnimInstanceClass);

    // 애니메이션 재생 위치
    void SetAnimationPlaybackTime(float InTime);
    float GetAnimationPlaybackTime() const;

    // 애니메이션 정보
    float GetAnimationPlayLength() const;
    float GetAnimationFrameRate() const;

    // 재생 속도
    void SetAnimationPlayRate(float InPlayRate);
    float GetAnimationPlayRate() const;

    // 반복 재생
    void SetAnimationLooping(bool bInLoop);
    bool IsAnimationLooping() const;

private:
    UAnimSingleNodeInstance* GetSingleNodeAnimationInstance() const;
    UAnimTwoWayBlendInstance* GetTwoWayBlendAnimationInstance() const;
    void EvaluateAnimationPose();

private:
    UAnimInstance* AnimInstance = nullptr;

    bool bPlayingAnimation = false;
};