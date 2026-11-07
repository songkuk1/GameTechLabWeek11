#include "EnginePCH.h"
#include "AnimationSequenceViewerTab.h"

#include "Engine/World.h"
#include "Animation/AnimSequence.h"

#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "Component/SkeletalMeshComponent.h"

#include "Editor/Viewports/PreviewViewportPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Viewports/ViewportCameraMath.h"

#include "Camera/CameraTypes.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float FramingPitchDegrees = 20.0f;
	FEditorTabSettings MakeAnimationSequenceViewerSettings(const UAnimSequence* Animation)
	{
		const FString AnimationPath = Animation ? Animation->GetPath() : "Unknown";
		const size_t Separator = AnimationPath.rfind("::");
		const FString AnimationName = Separator == FString::npos ? AnimationPath : AnimationPath.substr(Separator + 2);
		FEditorTabSettings Settings;
		Settings.Title = "Animation: " + AnimationName;
		Settings.DockSpaceName = "AnimationSequenceViewerDock_" + AnimationPath;
		Settings.bClosable = true;
		Settings.bIsolatePanels = true;
		return Settings;
	}
}

FAnimationSequenceViewerTab::FAnimationSequenceViewerTab(USkeletalMesh* InSkeletalMesh, UAnimSequence* InAnimation) : FEditorTab(MakeAnimationSequenceViewerSettings(InAnimation)), SkeletalMesh(InSkeletalMesh), Animation(InAnimation)
{
	PreviewScene = MakeUnique<FPreviewScene>();
	PreviewActor = Cast<ASkeletalMeshActor>(PreviewScene->GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));
	USkeletalMeshComponent* MeshComponent = PreviewActor->GetSkeletalMeshComponent();
	MeshComponent->SetSkeletalMesh(SkeletalMesh);
	MeshComponent->PlayAnimation(Animation);
	ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("AnimationViewport##") + Animation->GetPath(), [this]() {DrawToolbar();});
}

FAnimationSequenceViewerTab::~FAnimationSequenceViewerTab()
{
	Panels.Empty();
	ViewportPanel = nullptr;
	PreviewActor = nullptr;
}

void FAnimationSequenceViewerTab::Tick(float DeltaTime)
{
	FEditorTab::Tick(DeltaTime);
	USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();
	if (bPlaying)
	{
		Component->TickAnimation(DeltaTime);
		if (!Component->IsAnimationLooping())
		{
			const float CurrentTime = Component->GetAnimationPlaybackTime();
			const float Duration = Component->GetAnimationPlayLength();
			const float PlayRate = Component->GetAnimationPlayRate();
			if (PlayRate > 0.0f && CurrentTime >= Duration)
			{
				bPlaying = false;
			}
			else if (PlayRate < 0.0f && CurrentTime <= 0.0f)
			{
				bPlaying = false;
			}
		}
	}

	if (!bFramed && ViewportPanel->GetViewport()->IsVisible())
	{
		FrameMesh();
		bFramed = true;
	}
	GetWorld()->GetScene().UpdateAllTransforms();
}

void FAnimationSequenceViewerTab::RouteInput(const FViewportInput& BaseInput)
{
	ViewportPanel->RouteInput(BaseInput);
}

void FAnimationSequenceViewerTab::CollectViews(TArray<FEditorTabView>& OutViews)
{
	FEditorTabView View;
	View.Viewport = ViewportPanel->GetViewport();
	View.OverlayClient = ViewportPanel->GetEditorClient();
	OutViews.Add(View);
}

void FAnimationSequenceViewerTab::DrawToolbar()
{
	USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();
	if (!Component || !Animation)
	{
		return;
	}
	const float Duration = Component->GetAnimationPlayLength();
	const float FrameRate = Component->GetAnimationFrameRate();

	if (ImGui::Button(bPlaying ? "Pause" : "Play"))
	{
		if (!bPlaying && !bLooping && Duration > 0.0f && Component->GetAnimationPlaybackTime() >= Duration)
		{
			Component->SetAnimationPlaybackTime(0.0f);
		}
		bPlaying = !bPlaying;
	}
	ImGui::SameLine();

	if (ImGui::Button("Restart"))
	{
		Component->SetAnimationPlaybackTime(0.0f);
		bPlaying = true;
	}
	ImGui::SameLine();

	ImGui::BeginDisabled(FrameRate <= 0.0f);
	if (ImGui::Button("< Frame"))
	{
		bPlaying = false;
		const float CurrentTime = Component->GetAnimationPlaybackTime();
		Component->SetAnimationPlaybackTime(CurrentTime - (1.0f / FrameRate));
	}
	ImGui::SameLine();

	if (ImGui::Button("Frame >"))
	{
		bPlaying = false;
		const float CurrentTime = Component->GetAnimationPlaybackTime();
		Component->SetAnimationPlaybackTime(CurrentTime + (1.0f / FrameRate));
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	static const char* SpeedLabels[] = { "0.25x", "0.5x", "1.0x", "1.5x", "2.0x" };
	static const float SpeedValues[] = { 0.25f, 0.5f, 1.0f, 1.5f, 2.0f };

	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::BeginCombo("Speed", SpeedLabels[PlaybackSpeedIndex]))
	{
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const bool bSelected = PlaybackSpeedIndex == Index;
			if (ImGui::Selectable(SpeedLabels[Index], bSelected))
			{
				PlaybackSpeedIndex = Index;
				Component->SetAnimationPlayRate(SpeedValues[Index]);
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Checkbox("Loop", &bLooping))
	{
		Component->SetAnimationLooping(bLooping);
	}

	const float CurrentTime = Component->GetAnimationPlaybackTime();
	const int32 TotalFrames = std::max(1, Animation->GetDataModel()->NumberOfFrames);
	int32 CurrentFrame = 0;
	if (FrameRate > 0.0f)
	{
		CurrentFrame = std::clamp(static_cast<int32>(std::floor(CurrentTime * FrameRate)), 0, TotalFrames - 1);
	}

	ImGui::Text("Frame %d / %d     %.3f / %.3f sec", CurrentFrame + 1, TotalFrames, CurrentTime, Duration);

	if (Duration > 0.0f)
	{
		float ScrubTime = CurrentTime;
		ImGui::SetNextItemWidth(-1.0f);

		if (ImGui::SliderFloat("##AnimationTimeline", &ScrubTime, 0.0f, Duration, "%.3f sec"))
		{
			bPlaying = false;
			Component->SetAnimationPlaybackTime(ScrubTime);
		}
	}
	else
	{
		ImGui::TextDisabled("Animation has no valid duration.");
	}
}

void FAnimationSequenceViewerTab::FrameMesh()
{
	USkeletalMeshComponent* MeshComponent = PreviewActor->GetSkeletalMeshComponent();
	const FBox Bounds = SkeletalMesh->GetMeshData().AABB.GetWorldAABB(MeshComponent->GetWorldMatrix());
	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
	const float Radius = std::sqrt(HalfExtent.X * HalfExtent.X + HalfExtent.Y * HalfExtent.Y + HalfExtent.Z * HalfExtent.Z);
	const FMinimalViewInfo DefaultView;
	FViewCamera Camera{};
	Camera.Projection = { .Mode = EProjectionMode::Perspective, .FovDegrees = DefaultView.FOV, .OrthoWidth = DefaultView.OrthoWidth, .NearClip = DefaultView.NearClip, .FarClip = std::max(DefaultView.FarClip, Radius * 20.0f) };
	Camera.Transform.Rotation = ViewportCameraMath::MakeCameraRotation(0.0f, -FramingPitchDegrees);
	FEditorViewportClient* Client = ViewportPanel->GetEditorClient();
	Client->InitializeCamera(Camera, EMultipleViewportsCameraPreset::Perspective);
	Client->FocusOn(Center, Radius, ViewportPanel->GetViewport()->GetOutputRect());
}
