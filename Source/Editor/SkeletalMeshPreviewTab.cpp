#pragma once
#include "EnginePCH.h"
#include "SkeletalMeshPreviewTab.h"
#include "Engine/World.h"
#include "Editor/Viewports/PreviewViewportPanel.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "Animation/Skeleton.h"
#include "Input/InputSystem.h"
#include "Camera/CameraTypes.h"
#include "Core/PathUtils.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Viewports/ViewportCameraMath.h"
#include "SkeletonTreePanel.h"
#include "UObject/UObjectIterator.h"
#include "Component/SkeletalMeshComponent.h"
#include "Collision/Ray.h"

#include <algorithm>
#include <cmath>
#include <commdlg.h>

namespace
{
	// Preview 카메라가 메시를 내려다보는 각도
	constexpr float FramingPitchDegrees = 20.0f;

	FEditorTabSettings MakeSkeletalMeshPreviewTabSettings()
	{
		// 기존 imgui.ini 배치를 그대로 쓰도록 DockSpace 이름을 유지하고, 패널도 클래스로 묶지 않는다
		FEditorTabSettings Settings;
		Settings.Title = "Skeletal Mesh Preview";
		Settings.DockSpaceName = "SkeletalMeshPreviewDock";
		Settings.bClosable = true;
		Settings.bIsolatePanels = true;
		return Settings;
	}

	int32 FindRootBoneIndex(const USkeleton* Skeleton)
	{
		if (!Skeleton)
			return -1;

		const TArray<FModelBone>& Bones = Skeleton->GetBones();
		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = Bones[BoneIndex].ParentBoneIndex;
			if (ParentIndex < 0 || ParentIndex >= Bones.Num())
				return BoneIndex;
		}

		return -1;
	}
}


FSkeletalMeshPreviewTab::FSkeletalMeshPreviewTab(USkeletalMesh* InSkeletalMesh)
	: FEditorTab(MakeSkeletalMeshPreviewTabSettings()), SkeletalMesh(InSkeletalMesh)
{
	PreviewScene = MakeUnique<FPreviewScene>();
	MeshActor = Cast<ASkeletalMeshActor>(PreviewScene->GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));
	MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh);

	if (const USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
	{
		PoseSavePath = Skeleton->GetPosePath();
	}

	//TODO: 본을 선택했을때, 투명한 액터를 본위치로 이동시켜서 본을 선택한것처럼 보이게 한다.
	BoneGizmoActor = GetWorld()->SpawnActor(AActor::StaticClass());
	BoneGizmo.SetMode(EGizmoMode::Rotation);

	// AddPanel이 Init까지 부르므로 여기서 다시 부르지 않는다
	SkeletonTreePanel = AddPanel<FSkeletonTreePanel>(SkeletalMesh, FString("Skeleton Tree##") + SkeletalMesh->GetPath());
	SkeletonTreePanel->SetBoneSelectedCallback([this](int32 BoneIndex) { OnBoneSelected(BoneIndex); });

	ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("Viewport##") + SkeletalMesh->GetPath(), [this]()
		{
			DrawToolbar();
		});
}

FSkeletalMeshPreviewTab::~FSkeletalMeshPreviewTab()
{
	BoneGizmo.SetTarget(nullptr);
	Panels.Empty();
	ViewportPanel = nullptr;
	SkeletonTreePanel = nullptr;

}

// 포즈 저장 상태 및 공용 스켈레탈 메시 표시 설정 편집
void FSkeletalMeshPreviewTab::DrawToolbar()
{
	USkeletalMeshComponent* PreviewComponent = MeshActor->GetSkeletalMeshComponent();
	const USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	const int32 DefaultHeatmapBoneIndex = FindRootBoneIndex(Skeleton);
	const int32 HeatmapBoneIndex = SelectedBone >= 0 ? SelectedBone : DefaultHeatmapBoneIndex;
	const bool bHasBones = DefaultHeatmapBoneIndex >= 0;
	bool bShowBoneWeightHeatmap = PreviewComponent->IsBoneWeightHeatmapEnabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!bHasBones && !bShowBoneWeightHeatmap);

	if (ImGui::Checkbox("Bone Weight Heatmap", &bShowBoneWeightHeatmap))
	{
		if (bShowBoneWeightHeatmap)
			PreviewComponent->SetBoneWeightHeatmapBoneIndex(HeatmapBoneIndex);

		PreviewComponent->SetBoneWeightHeatmapEnabled(bShowBoneWeightHeatmap);
	}

	ImGui::EndDisabled();

	ImGui::SetItemTooltip("No bone selected: displays the first root bone's weights.");

	ImGui::BeginDisabled(PoseSavePath.empty() || BoneGizmo.IsUsing());
	const FString FileEntry = PathUtils::ToUtf8(PoseSavePath.filename()) + " ...###PoseFile";
	if (ImGui::Button(FileEntry.c_str()))
	{
		SelectPosePath();
	}
	ImGui::SetItemTooltip("%s", PathUtils::ToUtf8(PoseSavePath).c_str());
	ImGui::EndDisabled();

	const bool bNeedsSave = bPoseDirty || (Skeleton && PoseSavePath != Skeleton->GetPosePath());
	ImGui::SameLine();
	ImGui::BeginDisabled(!bNeedsSave || PoseSavePath.empty() || BoneGizmo.IsUsing());
	if (ImGui::Button("Save"))
	{
		SavePose();
	}
	ImGui::EndDisabled();

	if (bNeedsSave)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", bPoseSaveFailed ? "Pose save failed" : "Unsaved changes");
	}
}

// 포즈 저장 파일 경로 선택
void FSkeletalMeshPreviewTab::SelectPosePath()
{
	std::wstring FilePath(32768, L'\0');
	const auto FileName = PoseSavePath.filename().wstring();
	const auto InitialDirectory = PoseSavePath.parent_path().wstring();
	FileName.copy(FilePath.data(), FilePath.size() - 1);

	OPENFILENAMEW Dialog = {};
	Dialog.lStructSize = sizeof(Dialog);
	Dialog.hwndOwner = GetActiveWindow();
	Dialog.lpstrFile = FilePath.data();
	Dialog.nMaxFile = static_cast<DWORD>(FilePath.size());
	Dialog.lpstrFilter = L"Pose Files (*.pose)\0*.pose\0";
	Dialog.lpstrDefExt = L"pose";
	Dialog.lpstrInitialDir = InitialDirectory.c_str();
	Dialog.lpstrTitle = L"Choose Pose Save Location";
	Dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileNameW(&Dialog))
	{
		const std::filesystem::path SelectedPath(FilePath.c_str());
		if (_wcsicmp(SelectedPath.extension().c_str(), L".pose") != 0)
		{
			MessageBoxW(Dialog.hwndOwner, L"Choose a file with the .pose extension.", L"Pose Save Location", MB_OK | MB_ICONWARNING);
			return;
		}
		PoseSavePath = SelectedPath;
		bPoseSaveFailed = false;
	}
}

// 편집 포즈 저장 및 동일 메시 컴포넌트의 기본 포즈 갱신
void FSkeletalMeshPreviewTab::SavePose()
{
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	USkinnedMeshComponent* PreviewComponent = MeshActor->GetSkeletalMeshComponent();
	if (!Skeleton || !Skeleton->SaveDefaultPose(PreviewComponent->GetLocalPose(), PoseSavePath))
	{
		bPoseSaveFailed = true;
		HTR_LOG(Error, "Failed to save skeletal mesh pose: {}", SkeletalMesh->GetPath());
		return;
	}

	bPoseDirty = false;
	bPoseSaveFailed = false;
	PoseSavePath = Skeleton->GetPosePath();
	for (TObjectIterator<USkinnedMeshComponent> Component; Component; ++Component)
	{
		if (*Component != PreviewComponent && Component->GetSkeletalMesh() == SkeletalMesh)
		{
			Component->ResetPose();
			Component->UpdateBoneMatrices();
		}
	}
}

void FSkeletalMeshPreviewTab::PickBone(const FVector2& MousePosition)
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton || !SkeletonTreePanel)
		return;

	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	const TArray<FModelBone>& Bones = Skeleton->GetBones();
	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();

	const FViewport* Viewport = ViewportPanel->GetViewport();
	const FVector2 OutputSize = Viewport->GetOutputSize();
	if (OutputSize.X <= 0.0f || OutputSize.Y <= 0.0f)
		return;

	const FMatrix ComponentWorld = MeshComponent->GetWorldMatrix();
	const FMatrix ViewProjection = Viewport->PrepareView().ViewProjection;
	const int32 ScreenWidth = static_cast<int32>(OutputSize.X);
	const int32 ScreenHeight = static_cast<int32>(OutputSize.Y);

	// 각 노드가 어떤 Bone에 해당하는지 기록한다.
	TArray<int32> NodeToBone;
	NodeToBone.SetNum(Nodes.Num());
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		NodeToBone[NodeIndex] = -1;

	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		const int32 NodeIndex = Bones[BoneIndex].NodeIndex;
		if (NodeIndex >= 0 && NodeIndex < Nodes.Num())
			NodeToBone[NodeIndex] = BoneIndex;
	}

	// 각 Bone의 관절 위치를 화면 좌표로 변환한다.
	TArray<FVector2> ScreenJoints;
	TArray<uint8> bVisibleJoints;
	ScreenJoints.SetNum(Bones.Num());
	bVisibleJoints.SetNum(Bones.Num());

	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		bVisibleJoints[BoneIndex] = 0;

		const FMatrix JointWorld = MeshComponent->GetBoneComponentMatrix(BoneIndex) * ComponentWorld;
		const FVector JointPosition(JointWorld.M[3][0], JointWorld.M[3][1], JointWorld.M[3][2]);
		const FVector2 ScreenPosition = WorldToScreen(JointPosition, ViewProjection, ScreenWidth, ScreenHeight);

		if (!std::isfinite(ScreenPosition.X) || !std::isfinite(ScreenPosition.Y) || ScreenPosition.X < -1.0e20f || ScreenPosition.Y < -1.0e20f)
			continue;

		ScreenJoints[BoneIndex] = ScreenPosition;
		bVisibleJoints[BoneIndex] = 1;
	}

	int32 PickedBoneIndex = -1;
	float BestDistance = 10.0f;

	// 관절과 본을 잇는 선 중 마우스에 가장 가까운 본을 찾는다.
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		if (!bVisibleJoints[BoneIndex])
			continue;

		const FVector2& Joint = ScreenJoints[BoneIndex];
		const float DeltaX = MousePosition.X - Joint.X;
		const float DeltaY = MousePosition.Y - Joint.Y;
		const float JointDistance = std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);

		// 관절 중심을 클릭한 경우
		if (JointDistance < BestDistance)
		{
			BestDistance = JointDistance;
			PickedBoneIndex = BoneIndex;
		}

		// 부모 관절부터 현재 관절까지 이어지는 본 선을 검사한다.
		const int32 NodeIndex = Bones[BoneIndex].NodeIndex;
		const int32 ParentNodeIndex = Nodes[NodeIndex].ParentNodeIndex;

		if (ParentNodeIndex < 0 || ParentNodeIndex >= Nodes.Num())
			continue;

		const int32 ParentBoneIndex = NodeToBone[ParentNodeIndex];
		if (ParentBoneIndex < 0 || !bVisibleJoints[ParentBoneIndex])
			continue;

		const float SegmentDistance = DistanceToSegment(MousePosition, ScreenJoints[ParentBoneIndex], Joint);

		if (SegmentDistance <= 8.0f && SegmentDistance < BestDistance)
		{
			BestDistance = SegmentDistance;
			PickedBoneIndex = BoneIndex;
		}
	}

	if (PickedBoneIndex >= 0)
		SkeletonTreePanel->SetSelectedBone(PickedBoneIndex);
}

void FSkeletalMeshPreviewTab::Tick(float DeltaTime)
{
	FEditorTab::Tick(DeltaTime);
	// 화면 비율이 정해진 뒤(뷰포트가 처음 보일 때) 한 번만 카메라를 맞춘다
	if (!bFramed && ViewportPanel->GetViewport()->IsVisible())
	{
		FrameMesh();
		bFramed = true;
	}

	// 부모 본을 돌리면 자식 본도 움직이므로 매 프레임 따라간다. 드래그 중에는 Gizmo가 대상을 쥐고 있어 건드리지 않는다
	if (SelectedBone >= 0 && !BoneGizmo.IsUsing())
		UpdateBoneGizmoTarget();

	UpdateBoneGizmo(false);

	// Preview 월드는 엔진이 Tick하지 않는다. 에디터 월드처럼 트랜스폼만 렌더 전에 씬에 반영한다
	GetWorld()->GetScene().UpdateAllTransforms();
}

void FSkeletalMeshPreviewTab::FrameMesh()
{
	// 메시 바운드를 월드로 옮겨 중심과 감싸는 구 반지름을 구한다
	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	const FBox Bounds = SkeletalMesh->GetMeshData().AABB.GetWorldAABB(MeshComponent->GetWorldMatrix());
	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
	const float Radius = std::sqrt(HalfExtent.X * HalfExtent.X + HalfExtent.Y * HalfExtent.Y + HalfExtent.Z * HalfExtent.Z);

	// 기본 카메라는 FOV·Near·Far가 0이라 투영이 깨지므로 레벨 View와 같은 기본 투영으로 시작한다
	const FMinimalViewInfo DefaultView;
	FViewCamera Camera{};
	Camera.Projection = {
		.Mode = EProjectionMode::Perspective,
		.FovDegrees = DefaultView.FOV,
		.OrthoWidth = DefaultView.OrthoWidth,
		.NearClip = DefaultView.NearClip,
		// 메시가 커도 뒤쪽이 잘리지 않게 반지름에 맞춰 늘린다
		.FarClip = std::max(DefaultView.FarClip, Radius * 20.0f),
	};
	// 정면(+X)에서 살짝 내려다본다. 음의 Pitch가 아래를 본다
	Camera.Transform.Rotation = ViewportCameraMath::MakeCameraRotation(0.0f, -FramingPitchDegrees);

	// 방향은 그대로 두고 구가 화면에 들어오는 거리로 옮긴다. 레벨의 F 포커스와 같은 계산이다
	FEditorViewportClient* Client = ViewportPanel->GetEditorClient();
	Client->InitializeCamera(Camera, EMultipleViewportsCameraPreset::Perspective);
	Client->FocusOn(Center, Radius, ViewportPanel->GetViewport()->GetOutputRect());
}

void FSkeletalMeshPreviewTab::RouteInput(const FViewportInput& BaseInput)
{
	ViewportPanel->RouteInput(BaseInput);

	// W/E/R로 본 기즈모를 이동·회전·크기 모드로 바꾼다(언리얼과 같은 키).
	// 이 창 위에서만 받고, 우클릭 카메라 조작 중(W는 전진)이나 드래그 중에는 바꾸지 않는다
	if (BaseInput.bEnabled && ViewportPanel->IsHovered() && !ViewportPanel->IsCameraCaptured()
		&& !FInputSystem::IsMouseDown(EMouseButton::Right) && !BoneGizmo.IsUsing())
	{
		if (FInputSystem::IsKeyPressed(EKeyCode::W))
			BoneGizmo.SetMode(EGizmoMode::Location);
		else if (FInputSystem::IsKeyPressed(EKeyCode::E))
			BoneGizmo.SetMode(EGizmoMode::Rotation);
		else if (FInputSystem::IsKeyPressed(EKeyCode::R))
			BoneGizmo.SetMode(EGizmoMode::Scale);
	}

	UpdateBoneGizmo(BaseInput.bEnabled);
	if (BaseInput.bEnabled && ViewportPanel->IsHovered() && !ViewportPanel->IsCameraCaptured() && FInputSystem::IsMousePressed(EMouseButton::Left) && !BoneGizmo.IsUsing() && (!BoneGizmo.GetTarget() || BoneGizmo.GetHoveredAxis() < 0))
	{
		FVector2 MousePosition;
		if (ViewportPanel->GetMouseViewportPosition(MousePosition))
			PickBone(MousePosition);
	}
}

void FSkeletalMeshPreviewTab::UpdateBoneGizmoTarget()
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton || SelectedBone < 0 || SelectedBone >= Skeleton->GetBones().Num())
		return;

	// 본 행렬은 컴포넌트 기준이다. 행 벡터 규약이라 컴포넌트 월드 행렬을 오른쪽에 곱한다
	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	const FMatrix ComponentWorld = MeshComponent->GetWorldMatrix();
	BoneGizmoActor->GetRootComponent()->SetWorldTransform(
		MeshComponent->GetBoneComponentMatrix(SelectedBone).Multiply(ComponentWorld), false);
}

void FSkeletalMeshPreviewTab::UpdateBoneGizmo(const bool bInputEnabled)
{
	if (!BoneGizmo.GetTarget())
		return;
	// 드래그는 커서가 창 밖으로 나가도 버튼을 놓을 때까지 이어간다
	if (!BoneGizmo.IsUsing() && (!bInputEnabled || !ViewportPanel->IsHovered() || ViewportPanel->IsCameraCaptured()))
		return;

	FVector2 MousePosition;
	if (!ViewportPanel->GetMouseViewportPosition(MousePosition))
		return;

	const FViewport* Viewport = ViewportPanel->GetViewport();
	const FVector2 OutputSize = Viewport->GetOutputSize();
	USceneComponent* GizmoRoot = BoneGizmoActor->GetRootComponent();
	const FMatrix PreviousTargetWorld = GizmoRoot->GetWorldMatrix();
	BoneGizmo.Update(Viewport->Deproject(MousePosition), MousePosition, Viewport->PrepareView().ViewProjection,
		static_cast<int>(OutputSize.X), static_cast<int>(OutputSize.Y),
		FInputSystem::IsMouseDown(EMouseButton::Left), Viewport->GetCameraLocation(), Viewport->IsOrthographic());

	const FMatrix TargetWorld = GizmoRoot->GetWorldMatrix();
	if (!BoneGizmo.IsUsing())
	{
		return;
	}

	// Gizmo가 돌린 대상의 월드 행렬을 컴포넌트 기준으로 되돌려 본 포즈에 넣는다. 행 벡터 규약이라 역행렬을 오른쪽에 곱한다
	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	MeshComponent->SetBoneComponentMatrix(SelectedBone, TargetWorld * MeshComponent->GetWorldMatrix().Inverse());
	bPoseDirty = true;
	bPoseSaveFailed = false;
}

void FSkeletalMeshPreviewTab::CollectViews(TArray<FEditorTabView>& OutViews)
{
	// 보이는지는 엔진이 레벨 뷰포트와 같은 조건으로 거른다
	FEditorTabView View;
	View.Viewport = ViewportPanel->GetViewport();
	View.OverlayClient = ViewportPanel->GetEditorClient();
	View.Gizmo = &BoneGizmo;
	OutViews.Add(View);
}

void FSkeletalMeshPreviewTab::OnBoneSelected(int32 BoneIndex)
{
	SelectedBone = BoneIndex;
	USkeletalMeshComponent* PreviewComponent = MeshActor->GetSkeletalMeshComponent();

	if (SelectedBone < 0)
	{
		if (PreviewComponent->IsBoneWeightHeatmapEnabled())
		{
			const int32 RootBoneIndex = FindRootBoneIndex(SkeletalMesh->GetSkeleton());
			if (RootBoneIndex >= 0)
				PreviewComponent->SetBoneWeightHeatmapBoneIndex(RootBoneIndex);
			else
				PreviewComponent->SetBoneWeightHeatmapEnabled(false);
		}

		BoneGizmo.SetTarget(nullptr);
		return;
	}

	if (PreviewComponent->IsBoneWeightHeatmapEnabled())
	{
		PreviewComponent->SetBoneWeightHeatmapBoneIndex(SelectedBone);
	}

	UpdateBoneGizmoTarget();
	BoneGizmo.SetTarget(BoneGizmoActor->GetRootComponent());
}