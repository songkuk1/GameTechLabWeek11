#pragma once
#include "Editor/FEditorTab.h"
#include "Editor/Gizmo/Gizmo.h"
#include "PreviewScene.h"
#include "Render/SkeletalMesh.h"

class ASkeletalMeshActor;
class AActor;
class FPreviewViewportPanel;
class FSkeletonTreePanel;
class FPersonaDetailsPanel;
class UTexture2D;
class UAnimSequence;
class USkeleton;

class FSkeletalPreviewTabBase : public FEditorTab
{
public:
	explicit FSkeletalPreviewTabBase(const FEditorTabSettings& InSettings, USkeletalMesh* InSkeletalMesh);
	virtual ~FSkeletalPreviewTabBase() override;

	FSkeletalPreviewTabBase(const FSkeletalPreviewTabBase&) = delete;
	FSkeletalPreviewTabBase& operator=(const FSkeletalPreviewTabBase&) = delete;

	void Tick(float DeltaTime) override;
	void RouteInput(const FViewportInput& BaseInput) override;
	void CollectViews(TArray<FEditorTabView>& OutViews) override;

	void OnBoneSelected(int32 BoneIndex);
	void UpdateBoneGizmoTarget();
	void UpdateBoneGizmo(bool bInputEnabled);
	void PickBoneFromViewport(const FVector2& MousePos);
	UWorld* GetWorld() const override { return PreviewScene->GetWorld(); }

	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
	FPersonaDetailsPanel* GetDetailsPanel() const { return DetailsPanel; }

	virtual USkeleton* GetCurrentSkeleton() const;
	TArray<UAnimSequence*> GetCompatibleAnimSequences() const;

	bool IsShowMesh() const { return bShowMesh; }
	bool IsShowBoneWeightHeatmap() const { return bShowBoneWeightHeatmap; }
	EBoneDrawMode GetBoneDrawMode() const { return BoneDrawMode; }
	bool IsShowGrid() const { return bShowGrid; }
	float GetCameraSpeed() const { return CameraSpeed; }
	void SetCameraSpeed(float InSpeed) { CameraSpeed = InSpeed; }

protected:
	virtual void DrawToolbar() {}
	void DrawCommonViewToolbar();
	virtual void OnBoneTransformModified() {}
	virtual void OnBoneGizmoDragStarted() {}

	virtual void BuildDefaultLayout(ImGuiID DockSpaceRoot) override;

	void FrameMesh();

	bool bFramed = false;
	FPreviewViewportPanel* ViewportPanel = nullptr;
	FSkeletonTreePanel* SkeletonTreePanel = nullptr;
	FPersonaDetailsPanel* DetailsPanel = nullptr;

	USkeletalMesh* SkeletalMesh = nullptr;
	TUniquePtr<FPreviewScene> PreviewScene;
	ASkeletalMeshActor* MeshActor = nullptr;

	AActor* BoneGizmoActor = nullptr;
	FGizmo BoneGizmo;
	int32 SelectedBone = -1;

	UTexture2D* ShowFlagsIcon = nullptr;
	bool bShowMesh = true;
	bool bShowBoneWeightHeatmap = false;
	bool bUseProceduralHeatmap = false;
	bool bShowHeatmapWireframe = true;
	bool bHeatmapWireframeTinted = true;
	EBoneDrawMode BoneDrawMode = EBoneDrawMode::AllHierarchy;
	bool bShowGrid = true;
	int CurrentViewModeIndex = 0; // 0: Lit, 1: Unlit, 2: Wireframe
	float CameraSpeed = 4.0f;
	bool bEditingCameraSpeed = false;
	bool bFocusCameraSpeedInput = false;
};
