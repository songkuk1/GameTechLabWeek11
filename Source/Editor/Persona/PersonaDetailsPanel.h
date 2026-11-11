#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include <functional>

class USkeletalMesh;
class UAnimSequence;
class USkeletalMeshComponent;

class FPersonaDetailsPanel : public IEditorPanel
{
public:
	explicit FPersonaDetailsPanel(FString InPanelName);
	virtual ~FPersonaDetailsPanel() override = default;

	virtual bool Init() override { return true; }
	virtual void Tick(float DeltaTime) override {}
	virtual void OnRender() override;

	virtual const char* GetPanelName() const override { return PanelName.c_str(); }

	void SetTargetMesh(USkeletalMesh* InMesh) { SkeletalMesh = InMesh; }
	void SetTargetSequence(UAnimSequence* InSequence) { AnimSequence = InSequence; }
	void SetSelectedNotify(int32 InNotifyIndex) { SelectedNotifyIndex = InNotifyIndex; }
	void SetSelectedBone(int32 InBoneIndex) { SelectedBoneIndex = InBoneIndex; }

	void SetMeshComponent(USkeletalMeshComponent* InMeshComp) { MeshComponent = InMeshComp; }
	void SetOnBoneModified(std::function<void()> InCallback) { OnBoneModified = std::move(InCallback); }

	int32 GetSelectedNotifyIndex() const { return SelectedNotifyIndex; }

private:
	void DrawAssetDetails();
	void DrawNotifyDetails();
	void DrawBoneDetails();

	FString PanelName;
	USkeletalMesh* SkeletalMesh = nullptr;
	UAnimSequence* AnimSequence = nullptr;
	USkeletalMeshComponent* MeshComponent = nullptr;
	std::function<void()> OnBoneModified;

	int32 SelectedNotifyIndex = -1;
	int32 SelectedBoneIndex = -1;
};
