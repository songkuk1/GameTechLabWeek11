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
	void DrawNotifyTrack(float Duration);
	void DrawNotifyDetails(float Duration);
	void SelectNotify(int32 Index);
private:
	TArray<UAnimSequence*> AvailableAnimations;
	USkeletalMesh* SkeletalMesh = nullptr;
	UAnimSequence* Animation = nullptr;
	TUniquePtr<FPreviewScene> PreviewScene;
	ASkeletalMeshActor* PreviewActor = nullptr;
	FPreviewViewportPanel* ViewportPanel = nullptr;
	UAnimSequence* BlendAnimation = nullptr;

	float BlendAlpha = 0.5f;
	bool bUseTwoWayBlend = false;
	int32 PlaybackSpeedIndex = 2;
	int32 SelectedNotifyIndex = -1;
	bool bDraggingNotify = false;
	char NotifyNameBuffer[128] = {};
	bool bLooping = true;
	bool bPlaying = true;
	bool bFramed = false;
};