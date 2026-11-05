#pragma once

#include "Component/SkinnedMeshComponent.h"

class USkeleton;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
	virtual const FModelMeshData* GetMeshData() const override;

	void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

private:
	USkeleton* Skeleton = nullptr;
};
