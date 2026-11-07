#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Animation/AnimNode_SequencePlayer.h"
class USkeleton;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
	void PlayAnimation(UAnimSequence* InSequence);
	void TickAnimation(float DeltaSeconds);
	
	virtual const FModelMeshData* GetMeshData() const override;

	void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	// 애니메이션 재생 위치
	void SetAnimationPlaybackTime(float InTime);
	float GetAnimationPlaybackTime() const { return AnimationPlayer.GetCurrentTime(); }

	// 애니메이션 정보
	float GetAnimationPlayLength() const { return AnimationPlayer.GetPlayLength(); }
	float GetAnimationFrameRate() const { return AnimationPlayer.GetFrameRate(); }

	// 재생 속도
	void SetAnimationPlayRate(float InPlayRate) { AnimationPlayer.SetPlayRate(InPlayRate); }
	float GetAnimationPlayRate() const { return AnimationPlayer.GetPlayRate(); }

	// 반복 재생
	void SetAnimationLooping(bool bInLoop) { AnimationPlayer.SetLooping(bInLoop); }
	bool IsAnimationLooping() const { return AnimationPlayer.IsLooping(); }

private:
	void EvaluateAnimationPose();

	USkeleton* Skeleton = nullptr;
	FAnimNode_SequencePlayer AnimationPlayer;
	bool bPlayingAnimation = false;
};
