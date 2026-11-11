#pragma once
#include "SkeletalPreviewTabBase.h"
#include <filesystem>

class FSkeletalMeshPreviewTab : public FSkeletalPreviewTabBase
{
public:
	explicit FSkeletalMeshPreviewTab(USkeletalMesh* InSkeletalMesh);
	virtual ~FSkeletalMeshPreviewTab() override = default;

	virtual const UObject* GetEditingAsset() const override { return SkeletalMesh; }

protected:
	virtual void DrawToolbar() override;
	virtual void OnBoneTransformModified() override;
	virtual void OnBoneGizmoDragStarted() override;

private:
	void SelectPosePath();
	void SavePose();

	bool bPoseDirty = false;
	bool bPoseSaveFailed = false;
	std::filesystem::path PoseSavePath;

	UAnimSequence* ActivePreviewAnim = nullptr;
};