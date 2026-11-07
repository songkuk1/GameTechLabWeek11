#pragma once

#include "Editor/FEditorTab.h"
#include "PreviewScene.h"
#include "Animation/AnimSequence.h"
class ASkeletalMeshActor;
class FPreviewViewportPanel;
class USkeletalMesh;

class FAnimationSequenceViewerTab : public FEditorTab
{
public:
	FAnimationSequenceViewerTab(USkeletalMesh* InSkeletalMesh, UAnimSequence* InAnimation);
	virtual ~FAnimationSequenceViewerTab() override;
	FAnimationSequenceViewerTab(const FAnimationSequenceViewerTab&) = delete;
	FAnimationSequenceViewerTab& operator=(const FAnimationSequenceViewerTab&) = delete;
	
	void Tick(float DeltaTime) override;
	void RouteInput(const FViewportInput& BaseInput) override;
	void CollectViews(TArray<FEditorTabView>& OutViews) override;

	const UObject* GetEditingAsset() const override { return Animation; }
	UWorld* GetWorld() const override { return PreviewScene ? PreviewScene->GetWorld() : nullptr; }
private:
	void DrawToolbar();
	void FrameMesh();
private:
	USkeletalMesh* SkeletalMesh = nullptr;
	UAnimSequence* Animation = nullptr;
	TUniquePtr<FPreviewScene> PreviewScene;
	ASkeletalMeshActor* PreviewActor = nullptr;
	FPreviewViewportPanel* ViewportPanel = nullptr;
	int32 PlaybackSpeedIndex = 2;
	bool bLooping = true;
	bool bPlaying = true;
	bool bFramed = false;
};