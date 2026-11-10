#pragma once

#include "Editor/FEditorTab.h"
#include "PreviewScene.h"
#include "Animation/AnimSequence.h"
#include "Math/Transform.h"

class ASkeletalMeshActor;
class FPreviewViewportPanel;
class USkeletalMesh;
class UBlendSpace1D;
class UAnimSequence;
class USkeletalMeshComponent;

enum class EAnimationPreviewMode : uint8
{
	Sequence,
	TwoWayBlend,
	BlendSpace
};

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
	void DrawCurveEditor(USkeletalMeshComponent* Component, float Duration, float CurrentTime);
	void ApplyPlaybackSettings(USkeletalMeshComponent* Component);
	bool SetPreviewMode(EAnimationPreviewMode InMode, USkeletalMeshComponent* Component);
	void SelectNotify(int32 Index);
	void DrawBlendSpaceControls(USkeletalMeshComponent* Component);
	
private:
	TArray<UAnimSequence*> AvailableAnimations;
	TArray<UAnimSequence*> BlendSpaceAnimations;
	USkeletalMesh* SkeletalMesh = nullptr;
	UAnimSequence* Animation = nullptr;
	TUniquePtr<FPreviewScene> PreviewScene;
	ASkeletalMeshActor* PreviewActor = nullptr;
	FPreviewViewportPanel* ViewportPanel = nullptr;
	UAnimSequence* BlendAnimation = nullptr;

	UBlendSpace1D* PreviewBlendSpace = nullptr;
	float BlendAlpha = 0.5f;
	EAnimationPreviewMode PreviewMode = EAnimationPreviewMode::Sequence;
	int32 PlaybackSpeedIndex = 3;
	int32 SelectedNotifyIndex = -1;
	bool bDraggingNotify = false;
	char NotifyNameBuffer[128] = {};
	bool bLooping = true;
	bool bPlaying = true;
	bool bFramed = false;
	float BlendInput = 0.0f;

	int32 SelectedCurveIndex = -1;

	FTransform InitialPreviewActorTransform;
};