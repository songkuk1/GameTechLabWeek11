#pragma once
#include "SkeletalPreviewTabBase.h"
#include "Animation/AnimSequence.h"

class UAnimSingleNodeInstance;
class FAnimTimelinePanel;

class FAnimSequencePreviewTab : public FSkeletalPreviewTabBase
{
public:
	explicit FAnimSequencePreviewTab(UAnimSequence* InAnimSequence);
	virtual ~FAnimSequencePreviewTab() override = default;

	virtual const UObject* GetEditingAsset() const override { return AnimSequence; }
	virtual FAnimSequencePreviewTab* AsAnimSequencePreviewTab() override { return this; }
	virtual USkeleton* GetCurrentSkeleton() const override;

	void SetAnimSequence(UAnimSequence* InAnimSequence);

protected:
	virtual void DrawToolbar() override;
	virtual void OnBoneGizmoDragStarted() override;
	virtual void OnBoneTransformModified() override;
	virtual void BuildDefaultLayout(ImGuiID DockSpaceRoot) override;

private:
	UAnimSequence* AnimSequence = nullptr;
	UAnimSingleNodeInstance* SingleNodeInstance = nullptr;
	FAnimTimelinePanel* TimelinePanel = nullptr;
	void SetPreviewMesh(USkeletalMesh* InMesh);
};