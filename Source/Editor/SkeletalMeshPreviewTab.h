#pragma once
#include "Editor/FEditorTab.h"
#include "Editor/Gizmo/Gizmo.h"
#include "PreviewScene.h"
#include "Render/SkeletalMesh.h"

#include <filesystem>

class ASkeletalMeshActor;
class AActor;
class FPreviewViewportPanel;
class FSkeletonTreePanel;

class FViewportsPanel;

class FSkeletalMeshPreviewTab : public FEditorTab
{
public:
	explicit FSkeletalMeshPreviewTab(USkeletalMesh* InSkeletalMesh);
	virtual ~FSkeletalMeshPreviewTab() override;
	FSkeletalMeshPreviewTab(const FSkeletalMeshPreviewTab&) = delete;
	FSkeletalMeshPreviewTab& operator=(const FSkeletalMeshPreviewTab&) = delete;

	void Tick(float DeltaTime) override;

	//Input을 탭 안으로 전달
	void RouteInput(const FViewportInput& BaseInput) override;

	//View를 전달. 탭이 가지고 있는 ViewportPanel의 Viewport를 하나만 넘긴다
	void CollectViews(TArray<FEditorTabView>& OutViews) override;

	//Bone이 선택되면 Gizmo가 따라가도록 BoneGizmoActor를 본 위치로 옮긴다. 드래그 중에는 건드리지 않는다.
	// -1이면 선택을 해제한다
	void OnBoneSelected(int32 BoneIndex);
	void UpdateBoneGizmoTarget();
	void UpdateBoneGizmo(bool bInputEnabled);
	UWorld* GetWorld() const override { return PreviewScene->GetWorld(); }
	virtual const UObject* GetEditingAsset() const { return SkeletalMesh; };
private:
	void DrawToolbar();
	void SelectPosePath();
	void SavePose();

	// 메시 바운드가 화면에 들어오도록 Preview 카메라를 놓는다
	void FrameMesh();
	bool bFramed = false;
	bool bPoseDirty = false;
	bool bPoseSaveFailed = false;

	std::filesystem::path PoseSavePath;

	FPreviewViewportPanel* ViewportPanel = nullptr;
	const FGizmo* Gizmo = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
	//프리뷰 씬을 담는 PreviewScene. FSkeletalMeshPreviewTab이 소유한다.
	TUniquePtr<FPreviewScene> PreviewScene;
	ASkeletalMeshActor* MeshActor = nullptr;
	FSkeletonTreePanel* SkeletonTreePanel = nullptr;
	// 본은 컴포넌트가 아니라서, Gizmo가 따라갈 빈 액터를 본 위치에 둔다. 월드 소유
	AActor* BoneGizmoActor = nullptr;
	FGizmo BoneGizmo;
	// 선택 안 함은 -1이라 부호 있는 정수여야 한다
	int32 SelectedBone = -1;

};
