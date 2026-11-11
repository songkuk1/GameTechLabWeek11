#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"

class USkeleton;
class UAnimationGraph;
struct FAnimNotifyEvent;
enum class EAnimNotifyEventType : uint8;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    virtual ~USkeletalMeshComponent() override;

    USkeletalMeshComponent();
    virtual void TickComponent(float DeltaTime) override;

    void PlayAnimation(UAnimSequence* InSequence);

    void PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha);
    void SetAnimationBlendAlpha(float InAlpha);
    float GetAnimationBlendAlpha() const;

    void PlayBlendSpace(UBlendSpace1D* InBlendSpace, float InInput);
    void SetAnimationBlendInput(float InInput);
    float GetAnimationBlendInput() const;

    void TickAnimation(float DeltaSeconds);

    virtual const FModelMeshData* GetMeshData() const override;

    void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

    FPrimitiveSceneProxy* CreateSceneProxy() override;

    UAnimInstance* GetAnimInstance() const { return AnimInstance; }
    UAnimInstance* GetOrCreateAnimInstance();
    void SetAnimInstanceClass(UClass* InAnimInstanceClass);
    void SetAnimInstance(UAnimInstance* InAnimInstance);

    void SetAnimationRootNode(FAnimNode_Base* InRootNode);
    bool SetAnimationGraph(UAnimationGraph* InGraph);

    void HandleAnimNotify(const FName& NotifyName, EAnimNotifyEventType EventType, float DeltaSeconds);

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

    void SetRootMotionEnabled(bool bEnabled) { bRootMotionEnabled = bEnabled; }
    bool IsRootMotionEnabled() const { return bRootMotionEnabled; }
private:
    bool EnsureAnimInstance();
    void EvaluateAnimationPose();

private:
    UAnimInstance* AnimInstance = nullptr;
    bool bPlayingAnimation = false;
    bool bRootMotionEnabled = false;
};