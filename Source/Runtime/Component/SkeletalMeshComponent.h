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

private:
	USkeleton* Skeleton = nullptr;
	FAnimNode_SequencePlayer AnimationPlayer;
	bool bPlayingAnimation = false;
};
