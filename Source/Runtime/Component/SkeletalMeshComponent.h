#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Animation/UAnimInstance.h"

class USkeleton;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
	USkeletalMeshComponent();
	~USkeletalMeshComponent() override;
	void SetAnimationInstance(UAnimationInstance* InInstance);
	UAnimationInstance* GetAnimationInstance() const { return AnimInstance; }

	virtual const FModelMeshData* GetMeshData() const override;

	void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void TickComponent(float DeltaTime) override;

private:
	USkeleton* Skeleton = nullptr;
	UAnimationInstance* AnimInstance = nullptr;
};
