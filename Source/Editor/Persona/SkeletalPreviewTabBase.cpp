#include "EnginePCH.h"
#include "SkeletalPreviewTabBase.h"

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
#include "PersonaDetailsPanel.h"
#include "Asset/AssetManager.h"
#include "Collision/Ray.h"
#include "Animation/AnimSequence.h"
#include "UObject/UObjectIterator.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float FramingPitchDegrees = 20.0f;
}

FSkeletalPreviewTabBase::FSkeletalPreviewTabBase(const FEditorTabSettings& InSettings, USkeletalMesh* InSkeletalMesh)
	: FEditorTab(InSettings), SkeletalMesh(InSkeletalMesh)
{
	PreviewScene = MakeUnique<FPreviewScene>();
	MeshActor = Cast<ASkeletalMeshActor>(PreviewScene->GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));
	if (SkeletalMesh)
	{
		MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh);
	}

	BoneGizmoActor = GetWorld()->SpawnActor(AActor::StaticClass());
	BoneGizmo.SetMode(EGizmoMode::Rotation);

	const FString Suffix = InSettings.DockSpaceName.empty() ? (SkeletalMesh ? SkeletalMesh->GetPath() : "None") : InSettings.DockSpaceName;

	SkeletonTreePanel = AddPanel<FSkeletonTreePanel>(SkeletalMesh, FString("Skeleton Tree##") + Suffix);
	SkeletonTreePanel->SetBoneSelectedCallback([this](int32 BoneIndex) { OnBoneSelected(BoneIndex); });

	DetailsPanel = AddPanel<FPersonaDetailsPanel>(FString("Details##") + Suffix);
	DetailsPanel->SetTargetMesh(SkeletalMesh);
	DetailsPanel->SetMeshComponent(MeshActor ? MeshActor->GetSkeletalMeshComponent() : nullptr);
	DetailsPanel->SetOnBoneModified([this]()
		{
			UpdateBoneGizmoTarget();
			OnBoneTransformModified();
		});

	ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("Viewport##") + Suffix, [this]()
		{
			DrawToolbar();
		});
}

FSkeletalPreviewTabBase::~FSkeletalPreviewTabBase()
{
	BoneGizmo.SetTarget(nullptr);
	Panels.Empty();
	ViewportPanel = nullptr;
	SkeletonTreePanel = nullptr;
	DetailsPanel = nullptr;
}

void FSkeletalPreviewTabBase::BuildDefaultLayout(ImGuiID DockSpaceRoot)
{
	ImGuiID LeftNode = 0;
	ImGuiID RightNode = 0;
	ImGuiID CenterNode = 0;

	ImGui::DockBuilderSplitNode(DockSpaceRoot, ImGuiDir_Left, 0.20f, &LeftNode, &CenterNode);
	ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Right, 0.25f, &RightNode, &CenterNode);

	if (SkeletonTreePanel)
		ImGui::DockBuilderDockWindow(SkeletonTreePanel->GetPanelName(), LeftNode);
	if (DetailsPanel)
		ImGui::DockBuilderDockWindow(DetailsPanel->GetPanelName(), RightNode);
	if (ViewportPanel)
		ImGui::DockBuilderDockWindow(ViewportPanel->GetPanelName(), CenterNode);
}

void FSkeletalPreviewTabBase::Tick(float DeltaTime)
{
	FEditorTab::Tick(DeltaTime);

	if (!bFramed && ViewportPanel && ViewportPanel->GetViewport()->IsVisible())
	{
		FrameMesh();
		bFramed = true;
	}

	if (MeshActor && MeshActor->GetSkeletalMeshComponent())
	{
		MeshActor->GetSkeletalMeshComponent()->TickComponent(DeltaTime);

		if (BoneGizmo.IsUsing() && SelectedBone >= 0)
		{
			USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
			USceneComponent* GizmoRoot = BoneGizmoActor->GetRootComponent();
			MeshComponent->SetBoneComponentMatrix(SelectedBone, GizmoRoot->GetWorldMatrix() * MeshComponent->GetWorldMatrix().Inverse());
		}
	}

	if (SelectedBone >= 0 && !BoneGizmo.IsUsing())
	{
		UpdateBoneGizmoTarget();
	}

	GetWorld()->GetScene().UpdateAllTransforms();
}

void FSkeletalPreviewTabBase::FrameMesh()
{
	if (!SkeletalMesh || !MeshActor) return;

	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	const FBox Bounds = SkeletalMesh->GetMeshData().AABB.GetWorldAABB(MeshComponent->GetWorldMatrix());
	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
	const float Radius = std::sqrt(HalfExtent.X * HalfExtent.X + HalfExtent.Y * HalfExtent.Y + HalfExtent.Z * HalfExtent.Z);

	const FMinimalViewInfo DefaultView;
	FViewCamera Camera{};
	Camera.Projection = {
		.Mode = EProjectionMode::Perspective,
		.FovDegrees = DefaultView.FOV,
		.OrthoWidth = DefaultView.OrthoWidth,
		.NearClip = DefaultView.NearClip,
		.FarClip = std::max(DefaultView.FarClip, Radius * 20.0f),
	};
	Camera.Transform.Rotation = ViewportCameraMath::MakeCameraRotation(0.0f, -FramingPitchDegrees);

	FEditorViewportClient* Client = ViewportPanel->GetEditorClient();
	Client->InitializeCamera(Camera, EMultipleViewportsCameraPreset::Perspective);
	Client->FocusOn(Center, Radius, ViewportPanel->GetViewport()->GetOutputRect());
}

void FSkeletalPreviewTabBase::RouteInput(const FViewportInput& BaseInput)
{
	if (!ViewportPanel) return;

	FViewportInput TabInput = BaseInput;
	TabInput.MoveSpeed = CameraSpeed;
	ViewportPanel->RouteInput(TabInput);
	CameraSpeed = std::clamp(TabInput.MoveSpeed, 0.1f, 50.0f);

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
}

void FSkeletalPreviewTabBase::UpdateBoneGizmoTarget()
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton || SelectedBone < 0 || SelectedBone >= Skeleton->GetBones().Num() || BoneDrawMode == EBoneDrawMode::None)
		return;

	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	const FMatrix ComponentWorld = MeshComponent->GetWorldMatrix();
	BoneGizmoActor->GetRootComponent()->SetWorldTransform(
		MeshComponent->GetBoneComponentMatrix(SelectedBone).Multiply(ComponentWorld), false);
}

void FSkeletalPreviewTabBase::UpdateBoneGizmo(const bool bInputEnabled)
{
	if (!ViewportPanel) return;

	if (!BoneGizmo.IsUsing() && (!bInputEnabled || !ViewportPanel->IsHovered() || ViewportPanel->IsCameraCaptured()))
		return;

	FVector2 MousePosition;
	if (!ViewportPanel->GetMouseViewportPosition(MousePosition))
		return;

	const FViewport* Viewport = ViewportPanel->GetViewport();
	const FVector2 OutputSize = Viewport->GetOutputSize();

	// Gizmo를 조작 중이지 않고 카메라 캡처 중이 아닐 때 좌클릭으로 본 피킹
	if (BoneDrawMode != EBoneDrawMode::None && !BoneGizmo.IsUsing() && BoneGizmo.GetHoveredAxis() < 0
		&& FInputSystem::IsMousePressed(EMouseButton::Left))
	{
		PickBoneFromViewport(MousePosition);
	}

	if (!BoneGizmo.GetTarget() || BoneDrawMode == EBoneDrawMode::None) return;

	USceneComponent* GizmoRoot = BoneGizmoActor->GetRootComponent();

	const bool bWasUsing = BoneGizmo.IsUsing();

	BoneGizmo.Update(Viewport->Deproject(MousePosition), MousePosition, Viewport->PrepareView().ViewProjection,
		static_cast<int>(OutputSize.X), static_cast<int>(OutputSize.Y),
		FInputSystem::IsMouseDown(EMouseButton::Left), Viewport->GetCameraLocation(), Viewport->IsOrthographic());
	
	if (!bWasUsing && BoneGizmo.IsUsing())
	{
		OnBoneGizmoDragStarted();
	}
	if (!BoneGizmo.IsUsing()) return;

	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	MeshComponent->SetBoneComponentMatrix(SelectedBone, GizmoRoot->GetWorldMatrix() * MeshComponent->GetWorldMatrix().Inverse());
	OnBoneTransformModified();
}

void FSkeletalPreviewTabBase::PickBoneFromViewport(const FVector2& MousePos)
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton || !MeshActor || !ViewportPanel)
		return;

	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	if (!MeshComponent)
		return;

	const TArray<FModelBone>& Bones = Skeleton->GetBones();
	const FViewport* Viewport = ViewportPanel->GetViewport();
	const FVector2 OutputSize = Viewport->GetOutputSize();
	const FMatrix ViewProj = Viewport->PrepareView().ViewProjection;
	const FMatrix ComponentWorld = MeshComponent->GetWorldMatrix();

	auto ShouldConsiderBone = [&](int32 BoneIndex) -> bool
	{
		switch (BoneDrawMode)
		{
		case EBoneDrawMode::AllHierarchy:
			return true;
		case EBoneDrawMode::SelectedOnly:
			return SelectedBone >= 0 && BoneIndex == SelectedBone;
		case EBoneDrawMode::SelectedAndParents:
			if (SelectedBone < 0) return false;
			for (int32 P = SelectedBone; P >= 0 && P < Bones.Num(); P = Bones[P].ParentBoneIndex)
			{
				if (P == BoneIndex) return true;
			}
			return false;
		case EBoneDrawMode::None:
		default:
			return false;
		}
	};

	int32 BestBone = -1;
	float BestDistSq = 14.0f * 14.0f; // 14px 반경

	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		if (!ShouldConsiderBone(BoneIndex))
			continue;

		const FVector WorldPos = (MeshComponent->GetBoneComponentMatrix(BoneIndex) * ComponentWorld).TransformPosition(FVector::ZeroVector);
		const FVector2 Screen = WorldToScreen(WorldPos, ViewProj, static_cast<int>(OutputSize.X), static_cast<int>(OutputSize.Y));
		if (Screen.X == -FLT_MAX)
			continue;

		const float DistSq = (Screen.X - MousePos.X) * (Screen.X - MousePos.X) + (Screen.Y - MousePos.Y) * (Screen.Y - MousePos.Y);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestBone = BoneIndex;
		}
	}

	if (BestBone >= 0)
	{
		OnBoneSelected(BestBone);
		if (SkeletonTreePanel)
		{
			SkeletonTreePanel->SetSelectedBone(BestBone);
		}
	}
}

void FSkeletalPreviewTabBase::DrawCommonViewToolbar()
{
	if (!ShowFlagsIcon)
	{
		ShowFlagsIcon = UAssetManager::GetAssetByPath<UTexture2D>("Assets/Icons/Visible.png");
		if (!ShowFlagsIcon)
		{
			ShowFlagsIcon = UAssetManager::Get().LoadTexture("Assets/Icons/Visible.png");
		}
	}

	bool bOpenPopup = false;
	const float IconWidth = 24.0f;
	const float IconHeight = 22.0f;
	const float ChevronWidth = 6.0f;
	const float ChevronHeight = 4.0f;
	const float Spacing = 4.0f;
	const ImVec2 Padding(6.0f, 3.0f);
	const ImVec2 TotalSize(Padding.x * 2.0f + IconWidth + Spacing + ChevronWidth, IconHeight + Padding.y * 2.0f);

	const ImVec2 CursorScreenPos = ImGui::GetCursorScreenPos();
	const FString BtnId = "##ShowFlagsUnifiedBtn_" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (ImGui::InvisibleButton(BtnId.c_str(), TotalSize))
	{
		bOpenPopup = true;
	}

	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();

	if (bHovered)
	{
		ImGui::SetTooltip("Show Flags (Mesh, Bones, Grid)");
	}

	const ImU32 BgColor = bActive ? IM_COL32(35, 35, 35, 255) :
	                      bHovered ? IM_COL32(70, 70, 70, 255) :
	                      IM_COL32(50, 50, 50, 220);
	const ImU32 BorderColor = bHovered ? IM_COL32(110, 110, 110, 220) : IM_COL32(65, 65, 65, 180);
	const float Rounding = 4.0f;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 RectMax(CursorScreenPos.x + TotalSize.x, CursorScreenPos.y + TotalSize.y);
	DrawList->AddRectFilled(CursorScreenPos, RectMax, BgColor, Rounding);
	DrawList->AddRect(CursorScreenPos, RectMax, BorderColor, Rounding, 0, 1.0f);

	// Visible 아이콘 렌더링
	if (ShowFlagsIcon && ShowFlagsIcon->GetResource() && ShowFlagsIcon->GetResource()->GetRawSRV())
	{
		const ImVec2 IconMin(CursorScreenPos.x + Padding.x, CursorScreenPos.y + (TotalSize.y - IconHeight) * 0.5f);
		const ImVec2 IconMax(IconMin.x + IconWidth, IconMin.y + IconHeight);
		DrawList->AddImage((ImTextureID)ShowFlagsIcon->GetResource()->GetRawSRV(), IconMin, IconMax);
	}
	else
	{
		const ImVec2 TextPos(CursorScreenPos.x + Padding.x, CursorScreenPos.y + (TotalSize.y - ImGui::GetTextLineHeight()) * 0.5f);
		DrawList->AddText(TextPos, IM_COL32(220, 220, 220, 255), "Show");
	}

	// 드롭다운 Chevron 화살표 렌더링
	const float ChevronX = CursorScreenPos.x + Padding.x + IconWidth + Spacing;
	const float ChevronY = CursorScreenPos.y + (TotalSize.y - ChevronHeight) * 0.5f;
	const ImU32 ChevronColor = bActive ? IM_COL32(255, 255, 255, 255) :
	                           bHovered ? IM_COL32(230, 230, 230, 255) :
	                           IM_COL32(190, 190, 190, 255);
	const ImVec2 ChevronPts[3] = {
		ImVec2(ChevronX, ChevronY),
		ImVec2(ChevronX + ChevronWidth * 0.5f, ChevronY + ChevronHeight),
		ImVec2(ChevronX + ChevronWidth, ChevronY)
	};
	DrawList->AddPolyline(ChevronPts, 3, ChevronColor, 0, 1.5f);

	const FString PopupId = "ShowFlagsPopup_" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (bOpenPopup)
	{
		ImGui::OpenPopup(PopupId.c_str());
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
	if (ImGui::BeginPopup(PopupId.c_str()))
	{
		// 1. Mesh 메뉴
		if (ImGui::BeginMenu("Mesh"))
		{
			if (ImGui::MenuItem("Mesh", nullptr, bShowMesh))
			{
				bShowMesh = !bShowMesh;
				if (MeshActor && MeshActor->GetSkeletalMeshComponent())
				{
					MeshActor->GetSkeletalMeshComponent()->SetVisible(bShowMesh);
				}
			}
			if (ImGui::MenuItem("Bone Weight Heatmap", nullptr, bShowBoneWeightHeatmap))
			{
				bShowBoneWeightHeatmap = !bShowBoneWeightHeatmap;
			}
			if (bShowBoneWeightHeatmap)
			{
				ImGui::Separator();
				ImGui::MenuItem("  Show Wireframe Overlay", nullptr, &bShowHeatmapWireframe);
				if (bShowHeatmapWireframe)
				{
					if (ImGui::MenuItem("    Wireframe Color: Tinted (Weight)", nullptr, bHeatmapWireframeTinted))
					{
						bHeatmapWireframeTinted = true;
					}
					if (ImGui::MenuItem("    Wireframe Color: White", nullptr, !bHeatmapWireframeTinted))
					{
						bHeatmapWireframeTinted = false;
					}
				}
				if (ImGui::MenuItem("  Mode: Texture (1D LUT)", nullptr, !bUseProceduralHeatmap))
				{
					bUseProceduralHeatmap = false;
				}
				if (ImGui::MenuItem("  Mode: Procedural (Math)", nullptr, bUseProceduralHeatmap))
				{
					bUseProceduralHeatmap = true;
				}
				ImGui::Separator();
			}
			ImGui::EndMenu();
		}

		// 2. Bones 메뉴
		if (ImGui::BeginMenu("Bones"))
		{
			if (ImGui::MenuItem("All Hierarchy", nullptr, BoneDrawMode == EBoneDrawMode::AllHierarchy))
			{
				BoneDrawMode = EBoneDrawMode::AllHierarchy;
				if (SelectedBone >= 0)
					BoneGizmo.SetTarget(BoneGizmoActor ? BoneGizmoActor->GetRootComponent() : nullptr);
			}
			if (ImGui::MenuItem("Selected Only", nullptr, BoneDrawMode == EBoneDrawMode::SelectedOnly))
			{
				BoneDrawMode = EBoneDrawMode::SelectedOnly;
				if (SelectedBone >= 0)
					BoneGizmo.SetTarget(BoneGizmoActor ? BoneGizmoActor->GetRootComponent() : nullptr);
			}
			if (ImGui::MenuItem("Selected and Parents", nullptr, BoneDrawMode == EBoneDrawMode::SelectedAndParents))
			{
				BoneDrawMode = EBoneDrawMode::SelectedAndParents;
				if (SelectedBone >= 0)
					BoneGizmo.SetTarget(BoneGizmoActor ? BoneGizmoActor->GetRootComponent() : nullptr);
			}
			if (ImGui::MenuItem("None", nullptr, BoneDrawMode == EBoneDrawMode::None))
			{
				BoneDrawMode = EBoneDrawMode::None;
				BoneGizmo.SetTarget(nullptr);
			}
			ImGui::EndMenu();
		}

		// 3. Grid 메뉴
		if (ImGui::BeginMenu("Grid"))
		{
			ImGui::MenuItem("Grid & Axes", nullptr, &bShowGrid);
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);

	// ---- View Mode Combo (Lit, Unlit, Wireframe) ----
	ImGui::SameLine(0.0f, 6.0f);
	const char* PreviewViewModes[] = { "Lit", "Unlit", "Wireframe" };
	ImGui::SetNextItemWidth(95.0f);
	if (ImGui::Combo("##PersonaViewMode", &CurrentViewModeIndex, PreviewViewModes, IM_ARRAYSIZE(PreviewViewModes)))
	{
		if (ViewportPanel && ViewportPanel->GetViewport())
		{
			FViewRenderMode Mode = ViewportPanel->GetViewport()->GetViewRenderMode();
			switch (CurrentViewModeIndex)
			{
			case 0: Mode.ViewMode = EViewMode::Lit; break;
			case 1: Mode.ViewMode = EViewMode::Unlit; break;
			case 2: Mode.ViewMode = EViewMode::Wireframe; break;
			default: break;
			}
			ViewportPanel->GetViewport()->SetViewRenderMode(Mode);
		}
	}

	// ---- Camera Speed UI ----
	ImGui::SameLine(0.0f, 6.0f);

	char SpeedStr[16];
	snprintf(SpeedStr, sizeof(SpeedStr), "%.1f", CameraSpeed);
	const float CamTextWidth = ImGui::CalcTextSize(SpeedStr).x;
	const float CamIconWidth = 18.0f;
	const float CamSpacing = 6.0f;
	const ImVec2 CamTotalSize(Padding.x * 2.0f + CamIconWidth + CamSpacing + CamTextWidth, TotalSize.y);

	if (bEditingCameraSpeed)
	{
		ImGui::SetNextItemWidth(std::max(CamTotalSize.x + 12.0f, 54.0f));
		if (bFocusCameraSpeedInput)
		{
			ImGui::SetKeyboardFocusHere();
			bFocusCameraSpeedInput = false;
		}
		const FString InputId = "##CameraSpeedDirectInput_" + std::to_string(reinterpret_cast<uintptr_t>(this));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, (TotalSize.y - ImGui::GetFontSize()) * 0.5f - 1.0f));
		if (ImGui::InputFloat(InputId.c_str(), &CameraSpeed, 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			bEditingCameraSpeed = false;
		}
		ImGui::PopStyleVar();

		if (ImGui::IsItemDeactivated())
		{
			bEditingCameraSpeed = false;
		}
		CameraSpeed = std::clamp(CameraSpeed, 0.1f, 50.0f);
	}
	else
	{
		const ImVec2 CamCursorScreenPos = ImGui::GetCursorScreenPos();
		const FString CamBtnId = "##CameraSpeedBtn_" + std::to_string(reinterpret_cast<uintptr_t>(this));
		ImGui::InvisibleButton(CamBtnId.c_str(), CamTotalSize);

		const bool bCamHovered = ImGui::IsItemHovered();
		const bool bCamActive = ImGui::IsItemActive();

		// 더블 클릭 시 직접 텍스트 입력 모드로 전환
		if (bCamHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			bEditingCameraSpeed = true;
			bFocusCameraSpeedInput = true;
		}
		// 클릭 & 좌우 드래그로 속도 실시간 조절
		else if (bCamActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
		{
			const float DragDelta = ImGui::GetIO().MouseDelta.x;
			CameraSpeed = std::clamp(CameraSpeed + DragDelta * 0.05f, 0.1f, 50.0f);
		}

		if (bCamHovered || bCamActive)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			ImGui::BeginTooltip();
			ImGui::Text("Camera Speed: %.1f", CameraSpeed);
			ImGui::TextDisabled("Drag left/right to adjust");
			ImGui::TextDisabled("Double-click to type value");
			ImGui::EndTooltip();
		}

		const ImU32 CamBgColor = bCamActive ? IM_COL32(35, 35, 35, 255) :
		                         bCamHovered ? IM_COL32(70, 70, 70, 255) :
		                         IM_COL32(50, 50, 50, 220);
		const ImU32 CamBorderColor = bCamHovered ? IM_COL32(110, 110, 110, 220) : IM_COL32(65, 65, 65, 180);

		const ImVec2 CamRectMax(CamCursorScreenPos.x + CamTotalSize.x, CamCursorScreenPos.y + CamTotalSize.y);
		DrawList->AddRectFilled(CamCursorScreenPos, CamRectMax, CamBgColor, Rounding);
		DrawList->AddRect(CamCursorScreenPos, CamRectMax, CamBorderColor, Rounding, 0, 1.0f);

		// 카메라 아이콘 그리기
		const ImVec2 CamCenter(CamCursorScreenPos.x + Padding.x + CamIconWidth * 0.5f, CamCursorScreenPos.y + TotalSize.y * 0.5f);
		const ImU32 CamIconColor = bCamActive ? IM_COL32(255, 255, 255, 255) :
		                           bCamHovered ? IM_COL32(240, 240, 240, 255) :
		                           IM_COL32(200, 200, 200, 255);
		const ImU32 CamLensColor = bCamActive ? IM_COL32(35, 35, 35, 255) :
		                           bCamHovered ? IM_COL32(65, 65, 65, 255) :
		                           IM_COL32(50, 50, 50, 255);

		// 카메라 바디
		DrawList->AddRectFilled(ImVec2(CamCenter.x - 7.5f, CamCenter.y - 4.5f), ImVec2(CamCenter.x + 7.5f, CamCenter.y + 5.5f), CamIconColor, 2.0f);
		// 상단 뷰파인더 턱
		DrawList->AddRectFilled(ImVec2(CamCenter.x - 4.0f, CamCenter.y - 7.0f), ImVec2(CamCenter.x + 0.5f, CamCenter.y - 4.5f), CamIconColor, 1.0f);
		// 렌즈 원
		DrawList->AddCircleFilled(ImVec2(CamCenter.x, CamCenter.y + 0.5f), 3.2f, CamLensColor);
		DrawList->AddCircle(ImVec2(CamCenter.x, CamCenter.y + 0.5f), 3.2f, CamIconColor, 12, 1.0f);
		DrawList->AddCircleFilled(ImVec2(CamCenter.x, CamCenter.y + 0.5f), 1.2f, CamIconColor);

		// 속도 숫자 텍스트
		const float TextStartX = CamCursorScreenPos.x + Padding.x + CamIconWidth + CamSpacing;
		const float TextStartY = CamCursorScreenPos.y + (TotalSize.y - ImGui::GetTextLineHeight()) * 0.5f;
		DrawList->AddText(ImVec2(TextStartX, TextStartY), IM_COL32(220, 220, 220, 255), SpeedStr);
	}

	ImGui::SameLine(0.0f, 8.0f);
	ImGui::Separator();
	ImGui::SameLine(0.0f, 8.0f);
}

void FSkeletalPreviewTabBase::CollectViews(TArray<FEditorTabView>& OutViews)
{
	if (!ViewportPanel) return;
	FEditorTabView View;
	View.Viewport = ViewportPanel->GetViewport();
	View.OverlayClient = (bShowGrid && ViewportPanel) ? ViewportPanel->GetEditorClient() : nullptr;
	View.Gizmo = (BoneDrawMode != EBoneDrawMode::None) ? &BoneGizmo : nullptr;
	View.SelectedBone = SelectedBone;
	View.BoneDrawMode = BoneDrawMode;
	View.bShowBoneWeightHeatmap = bShowBoneWeightHeatmap;
	View.bUseProceduralHeatmap = bUseProceduralHeatmap;
	View.bShowHeatmapWireframe = bShowHeatmapWireframe;
	View.bHeatmapWireframeTinted = bHeatmapWireframeTinted;
	OutViews.Add(View);
}

void FSkeletalPreviewTabBase::OnBoneSelected(int32 BoneIndex)
{
	SelectedBone = BoneIndex;
	if (DetailsPanel)
	{
		DetailsPanel->SetSelectedBone(BoneIndex);
	}

	if (SelectedBone < 0 || BoneDrawMode == EBoneDrawMode::None)
	{
		BoneGizmo.SetTarget(nullptr);
		return;
	}
	UpdateBoneGizmoTarget();
	BoneGizmo.SetTarget(BoneGizmoActor ? BoneGizmoActor->GetRootComponent() : nullptr);
}

USkeleton* FSkeletalPreviewTabBase::GetCurrentSkeleton() const
{
	return SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
}

TArray<UAnimSequence*> FSkeletalPreviewTabBase::GetCompatibleAnimSequences() const
{
	TArray<UAnimSequence*> Compatible;
	const USkeleton* MySkeleton = GetCurrentSkeleton();
	if (!MySkeleton)
	{
		return Compatible;
	}

	for (TObjectIterator<UAnimSequence> It; It; ++It)
	{
		UAnimSequence* Anim = *It;
		if (!Anim) continue;

		if (const USkeleton* AnimSkeleton = Anim->GetSkeleton())
		{
			if (MySkeleton->IsCompatibleWith(AnimSkeleton) || AnimSkeleton->IsCompatibleWith(MySkeleton))
			{
				Compatible.Add(Anim);
			}
		}
	}

	std::sort(Compatible.begin(), Compatible.end(), [](UAnimSequence* A, UAnimSequence* B)
	{
		return A->GetPath() < B->GetPath();
	});

	return Compatible;
}
