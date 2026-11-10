#pragma once

#include "EnginePCH.h"

#include "FEditorTab.h"
#include "Animation/UAnimSequence.h"
#include "Render/SkeletalMesh.h"
#include "Editor/Viewports/PreviewViewportPanel.h"
#include "PreviewScene.h"


class ASkeletalMeshActor;
class AActor;
class FPreviewViewportPanel;
class FViewportsPanel;
class FAnimTimelinePanel;



class FAnimSequencePreviewTab : public FEditorTab
{
public:
	FAnimSequencePreviewTab(UAnimSequence* InSequence, USkeletalMesh* InMesh);

	UWorld* GetWorld() const override { return PreviewScene->GetWorld(); }
	virtual const UObject* GetEditingAsset() const override { return AnimSequence; };

	//Input을 탭 안으로 전달
	void RouteInput(const FViewportInput& BaseInput) override;

	//View를 전달. 탭이 가지고 있는 ViewportPanel의 Viewport를 하나만 넘긴다
	void CollectViews(TArray<FEditorTabView>& OutViews) override;

	void Tick(float DeltaTime) override;
	float GetCurrentTime() const { return CurrentTime; }
	float GetPlayLength() const;
	bool IsPlaying() const { return bPlaying; }
	void Play();
	void Pause() { bPlaying = false; }
	void Stop();
	void SetCurrentTime(float Time);

	void FrameMesh();
	bool bFramed = false;

protected:
	void BuildDefaultLayout(ImGuiID DockSpaceRoot) override;

 private:
	void UpdatePreviewPose();
	float CurrentTime = 0.0f;
	UAnimSequence* AnimSequence;
	USkeletalMesh* SkeletalMesh;
    FPreviewViewportPanel* ViewportPanel = nullptr;
	FAnimTimelinePanel* TimelinePanel = nullptr;
    TUniquePtr<FPreviewScene> PreviewScene;
    ASkeletalMeshActor* MeshActor = nullptr;
	bool bPlaying = false;
};
