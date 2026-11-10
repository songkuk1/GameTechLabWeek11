#include "EnginePCH.h"
#include "FAnimSequencePreviewTab.h"
#include "AnimTimelinePanel.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "Editor/Viewports/ViewportCameraMath.h"
#include "Camera/CameraTypes.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float FramingPitchDegrees = 20.0f;

	FEditorTabSettings MakeAnimSequencePreviewTabSettings(const UAnimSequence* Sequence)
	{
		FEditorTabSettings Settings;
		const FString Path = Sequence ? Sequence->GetPath() : "";
		const size_t Separator = Path.find_last_of("/\\");
		const FString Name = Separator == FString::npos ? Path : Path.substr(Separator + 1);
		Settings.Title = "Animation: " + Name + "##" + Path;
		Settings.DockSpaceName = "AnimSequencePreviewTimelineDock##" + Path;
		Settings.bClosable = true;
		Settings.bIsolatePanels = true;
		return Settings;
	}
}

FAnimSequencePreviewTab::FAnimSequencePreviewTab(UAnimSequence* InSequence, USkeletalMesh* InMesh)
    : FEditorTab(MakeAnimSequencePreviewTabSettings(InSequence))
    , AnimSequence(InSequence)
    , SkeletalMesh(InMesh)
{
    PreviewScene = MakeUnique<FPreviewScene>();
    MeshActor = Cast<ASkeletalMeshActor>(PreviewScene->GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));
    MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh);

	ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("Anim Viewport##") + AnimSequence->GetPath(), []() {});
	TimelinePanel = AddPanel<FAnimTimelinePanel>(*this, FString("Timeline##") + AnimSequence->GetPath());
}

void FAnimSequencePreviewTab::BuildDefaultLayout(ImGuiID DockSpaceRoot)
{
	ImGuiID ViewportDock = 0;
	ImGuiID TimelineDock = 0;
	ImGui::DockBuilderSplitNode(DockSpaceRoot, ImGuiDir_Down, 0.25f, &TimelineDock, &ViewportDock);
	ImGui::DockBuilderDockWindow(ViewportPanel->GetPanelName(), ViewportDock);
	ImGui::DockBuilderDockWindow(TimelinePanel->GetPanelName(), TimelineDock);
}

float FAnimSequencePreviewTab::GetPlayLength() const
{
	const float PlayLength = AnimSequence ? AnimSequence->GetPlayLength() : 0.0f;
	return std::isfinite(PlayLength) && PlayLength > 0.0f ? PlayLength : 0.0f;
}

void FAnimSequencePreviewTab::Play()
{
	const float PlayLength = GetPlayLength();
	if (PlayLength <= 0.0f)
		return;
	if (CurrentTime >= PlayLength)
		CurrentTime = 0.0f;
	bPlaying = true;
}

void FAnimSequencePreviewTab::Stop()
{
	bPlaying = false;
	CurrentTime = 0.0f;
}

void FAnimSequencePreviewTab::SetCurrentTime(float Time)
{
	if (!std::isfinite(Time))
		return;
	bPlaying = false;
	CurrentTime = std::clamp(Time, 0.0f, GetPlayLength());
}

void FAnimSequencePreviewTab::RouteInput(const FViewportInput& BaseInput)
{
	ViewportPanel->RouteInput(BaseInput);
}

void FAnimSequencePreviewTab::CollectViews(TArray<FEditorTabView>& OutViews)
{
	// 보이는지는 엔진이 레벨 뷰포트와 같은 조건으로 거른다
	FEditorTabView View;
	View.Viewport = ViewportPanel->GetViewport();
	View.OverlayClient = ViewportPanel->GetEditorClient();
	OutViews.Add(View);
}

void FAnimSequencePreviewTab::Tick(float DeltaTime)
{
	FEditorTab::Tick(DeltaTime);

	// 화면 비율이 정해진 뒤(뷰포트가 처음 보일 때) 한 번만 카메라를 맞춘다
	if (!bFramed && ViewportPanel->GetViewport()->IsVisible())
	{
		FrameMesh();
		bFramed = true;
	}

	const float PlayLength = GetPlayLength();
	if (PlayLength <= 0.0f)
		Stop();
	else if (bPlaying && std::isfinite(DeltaTime) && DeltaTime > 0.0f)
	{
		CurrentTime = std::min(CurrentTime + DeltaTime, PlayLength);
		if (CurrentTime >= PlayLength)
			bPlaying = false;
	}

	UpdatePreviewPose();

	// Preview 월드는 엔진이 Tick하지 않는다. 에디터 월드처럼 트랜스폼만 렌더 전에 씬에 반영한다
	GetWorld()->GetScene().UpdateAllTransforms();
}

void FAnimSequencePreviewTab::UpdatePreviewPose()
{
	if (!AnimSequence || !MeshActor)
		return;

	USkeletalMeshComponent* MeshComponent = MeshActor->GetSkeletalMeshComponent();
	if (!MeshComponent || !MeshComponent->GetSkeletalMesh()
		|| MeshComponent->GetReferencePose().IsEmpty())
		return;

	// 트랙이 없는 본은 기본 자세를 유지하도록 매번 기본 포즈에서 시작한다.
	TArray<PoseTransform> SampledPose = MeshComponent->GetReferencePose();
	AnimSequence->CalculateCurrentPose(CurrentTime, false, SampledPose);
	MeshComponent->ApplyLocalPose(SampledPose);
}

void FAnimSequencePreviewTab::FrameMesh()
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
