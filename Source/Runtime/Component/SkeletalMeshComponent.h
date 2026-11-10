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
	void PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping = true, bool bReverse = false);
	void PlayAnimation(const FString& Path, bool bLooping = true, bool bReverse = false);
	// Actor 적용 전의 월드 기준 변화 행렬.
	const FMatrix& GetWorldRootMotionMatrix() const { return WorldRootMotionMatrix; }

	virtual const FModelMeshData* GetMeshData() const override;

	void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void TickComponent(float DeltaTime) override;

	void ApplyLocalPose(const TArray<PoseTransform>& InPose);

private:
	void UpdateWorldRootMotion();
	FMatrix WorldRootMotionMatrix = FMatrix::Identity;
	USkeleton* Skeleton = nullptr;
	UAnimationInstance* AnimInstance = nullptr;
};
