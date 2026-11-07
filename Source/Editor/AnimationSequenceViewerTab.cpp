#include "EnginePCH.h"
#include "AnimationSequenceViewerTab.h"

#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

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

static const float SpeedValues[] =
{
	0.25f, 0.5f, 1.0f, 1.5f, 2.0f
};

FAnimationSequenceViewerTab::FAnimationSequenceViewerTab(USkeletalMesh* InSkeletalMesh, UAnimSequence* InAnimation) : FEditorTab(MakeAnimationSequenceViewerSettings(InAnimation)), SkeletalMesh(InSkeletalMesh), Animation(InAnimation)
{
	PreviewScene = MakeUnique<FPreviewScene>();
	PreviewActor = Cast<ASkeletalMeshActor>(PreviewScene->GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));
	USkeletalMeshComponent* MeshComponent = PreviewActor->GetSkeletalMeshComponent();
	MeshComponent->SetSkeletalMesh(SkeletalMesh);
	MeshComponent->PlayAnimation(Animation);
	const FString AnimationPrefix = SkeletalMesh->GetPath() + "::";
	for (TObjectIterator<UAnimSequence> It; It; ++It)
	{
		UAnimSequence* Candidate = *It;
		if (Candidate && Candidate != Animation && Candidate->GetPath().find(AnimationPrefix) == 0)
		{
			AvailableAnimations.Add(Candidate);
		}
	}
	std::sort(AvailableAnimations.begin(), AvailableAnimations.end(), [](const UAnimSequence* A, const UAnimSequence* B) {return A->GetPath() < B->GetPath();});
	if (!AvailableAnimations.IsEmpty())
	{
		BlendAnimation = AvailableAnimations[0];
	}

	BlendSpaceAnimations.Add(Animation);
	for (UAnimSequence* Candidate : AvailableAnimations)
	{
		BlendSpaceAnimations.Add(Candidate);
	}
	IdleAnimation = Animation;
	if (AvailableAnimations.Num() > 0)
	{
		WalkAnimation = AvailableAnimations[0];
		RunAnimation = AvailableAnimations[0];
	}

	if (AvailableAnimations.Num() > 1)
	{
		RunAnimation = AvailableAnimations[1];
	}

	PreviewBlendSpace = NewObject<UBlendSpace1D>();

	RebuildPreviewBlendSpace();

	ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("AnimationViewport##") + Animation->GetPath(), [this]() {DrawToolbar();});
}

FAnimationSequenceViewerTab::~FAnimationSequenceViewerTab()
{
	if (PreviewActor && PreviewBlendSpace)
	{
		// BlendSpaceInstance를 먼저 SingleNodeInstance로 교체한다.
		PreviewActor->GetSkeletalMeshComponent()->PlayAnimation(Animation);
	}

	delete PreviewBlendSpace;
	PreviewBlendSpace = nullptr;
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
	DrawBlendSpaceControls(Component);

	ImGui::TextUnformatted("Two-Way Blend");
	ImGui::BeginDisabled(BlendAnimation == nullptr);
	if (ImGui::Checkbox("Enable Blend", &bUseTwoWayBlend))
	{
		if (bUseTwoWayBlend)
		{
			bUseBlendSpace = false;
			Component->PlayBlendAnimation(Animation, BlendAnimation, BlendAlpha);
		}
		else
		{
			Component->PlayAnimation(Animation);
		}
	}
	ImGui::EndDisabled();

	if (BlendAnimation)
	{
		const FString CurrentBlendName = BlendAnimation->GetPath().substr(SkeletalMesh->GetPath().size() + 2);

		ImGui::SetNextItemWidth(250.0f);

		if (ImGui::BeginCombo("Source B", CurrentBlendName.c_str()))
		{
			for (UAnimSequence* Candidate : AvailableAnimations)
			{
				const FString Name = Candidate->GetPath().substr(SkeletalMesh->GetPath().size() + 2);

				const bool bSelected = Candidate == BlendAnimation;

				if (ImGui::Selectable(Name.c_str(), bSelected))
				{
					BlendAnimation = Candidate;

					if (bUseTwoWayBlend)
					{
						Component->PlayBlendAnimation(Animation, BlendAnimation, BlendAlpha);
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	if (bUseTwoWayBlend)
	{
		if (ImGui::SliderFloat("Blend Alpha", &BlendAlpha, 0.0f, 1.0f, "%.2f"))
		{
			Component->SetAnimationBlendAlpha(BlendAlpha);
		}
	}

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

	ImGui::Separator();
	ImGui::TextUnformatted("Notify Track");
	ImGui::BeginDisabled(Duration <= 0.0f);
	if (ImGui::Button("Add Notify"))
	{
		const int32 NewIndex = Animation->AddNotify(CurrentTime, 0.0f, FName("NewNotify"));
		SelectNotify(NewIndex);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!Animation->GetNotifies().IsValidIndex(SelectedNotifyIndex));
	if (ImGui::Button("Remove Notify"))
	{
		Animation->RemoveNotify(SelectedNotifyIndex);
		SelectNotify(-1);
	}
	ImGui::EndDisabled();
	DrawNotifyTrack(Duration);
	DrawNotifyDetails(Duration);
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

void FAnimationSequenceViewerTab::DrawNotifyTrack(float Duration)
{
	if (Duration <= 0.0f)
	{
		return;
	}
	TArray<FAnimNotifyEvent>& Notifies = Animation->GetMutableNotifies();
	constexpr float TrackHeight = 36.0f;
	const float Width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
	ImGui::InvisibleButton("##AnimationNotifyTrack", ImVec2(Width, TrackHeight));
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const float TrackLeft = Min.x + 8.0f;
	const float TrackRigth = Max.x - 8.0f;
	const float TrackWidth = std::max(1.0f, TrackRigth - TrackLeft);
	DrawList->AddRectFilled(Min, Max, IM_COL32(45, 48, 55, 255), 3.0f);
	DrawList->AddLine(ImVec2(TrackLeft, (Min.y + Max.y) * 0.5f), ImVec2(TrackRigth, (Min.y + Max.y) * 0.5f), IM_COL32(100,105,115,255));
	for (int32 Index = 0; Index < Notifies.Num(); ++Index)
	{
		const FAnimNotifyEvent& Notify = Notifies[Index];
		const float TriggerTime = std::clamp(Notify.TriggerTime, 0.0f, Duration);
		const float Alpha = TriggerTime / Duration;
		const float X = TrackLeft + TrackWidth * Alpha;
		const bool bSelected = Index == SelectedNotifyIndex;
		const ImU32 MarkerColor = bSelected ? IM_COL32(255, 190, 70, 255) : IM_COL32(80, 180, 255, 255);
		if (Notify.Duration > 0.0f)
		{
			const float EndTime = std::clamp(Notify.TriggerTime + Notify.Duration, TriggerTime, Duration);
			const float EndX = TrackLeft + TrackWidth * (EndTime / Duration);
			if (EndX > X)
			{
				DrawList->AddRectFilled(ImVec2(X, Min.y + 12.0f), ImVec2(EndX, Max.y - 8.0f), IM_COL32(65, 125, 175, 180), 2.0f);
			}
		}
		DrawList->AddLine(ImVec2(X, Min.y + 5.0f), ImVec2(X, Max.y - 5.0f), MarkerColor, bSelected ? 2.5f : 1.5f);
		DrawList->AddTriangleFilled(ImVec2(X - 5.0f, Min.y + 4.0f), ImVec2(X + 5.0f, Min.y + 4.0f), ImVec2(X, Min.y + 13.0f), MarkerColor);

		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			const float MouseX = ImGui::GetIO().MousePos.x;
			int32 NearestIndex = -1;
			float NearestDistance = 12.0f;
			for (int32 Index = 0; Index < Notifies.Num(); ++Index)
			{
				const float TriggerTime = std::clamp(Notifies[Index].TriggerTime, 0.0f, Duration);
				const float X = TrackLeft + TrackWidth * (TriggerTime / Duration);
				const float Distance = std::abs(MouseX - X);
				if (Distance < NearestDistance)
				{
					NearestDistance = Distance;
					NearestIndex = Index;
				}
			}
			SelectNotify(NearestIndex);
			bDraggingNotify = NearestIndex >= 0;
		}
	}
	if (bDraggingNotify && ImGui::IsMouseDown(ImGuiMouseButton_Left) && Notifies.IsValidIndex(SelectedNotifyIndex))
	{
		const float MouseX = ImGui::GetIO().MousePos.x;
		const float Alpha = std::clamp((MouseX - TrackLeft) / TrackWidth, 0.0f, 1.0f);
		FAnimNotifyEvent& Notify = Notifies[SelectedNotifyIndex];
		Notify.TriggerTime = Alpha * Duration;
		Notify.Duration = std::clamp(Notify.Duration, 0.0f, Duration - Notify.TriggerTime);
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		bDraggingNotify = false;
	}
}

void FAnimationSequenceViewerTab::DrawNotifyDetails(float Duration)
{
	TArray<FAnimNotifyEvent>& Notifies = Animation->GetMutableNotifies();
	if (!Notifies.IsValidIndex(SelectedNotifyIndex))
	{
		return;
	}
	FAnimNotifyEvent& Notify = Notifies[SelectedNotifyIndex];
	ImGui::Separator();
	ImGui::TextUnformatted("Selected Notify");

	if (ImGui::InputText("Name", NotifyNameBuffer, sizeof(NotifyNameBuffer)))
	{
		Notify.NotifyName = FName(NotifyNameBuffer);
	}
	float TriggerTime = Notify.TriggerTime;
	if (ImGui::DragFloat("Trigger Time", &TriggerTime, 0.01f, 0.0f, Duration, "%.3f sec"))
	{
		Notify.TriggerTime = std::clamp(TriggerTime, 0.0f, Duration);
		Notify.Duration = std::clamp(Notify.Duration, 0.0f, Duration - Notify.TriggerTime);
	}
	float NotifyDuration = Notify.Duration;
	const float MaxDuration = std::max(0.0f, Duration - Notify.TriggerTime);
	if (ImGui::DragFloat("Duration", &NotifyDuration, 0.01f, 0.0f, MaxDuration, "%.3f sec"))
	{
		Notify.Duration = std::clamp(NotifyDuration, 0.0f, MaxDuration);
	}
}

void FAnimationSequenceViewerTab::SelectNotify(int32 Index)
{
	const TArray<FAnimNotifyEvent>& Notifies = Animation->GetNotifies();
	if (!Notifies.IsValidIndex(Index))
	{
		SelectedNotifyIndex = -1;
		bDraggingNotify = false;
		std::fill(NotifyNameBuffer, NotifyNameBuffer + sizeof(NotifyNameBuffer), '\0');
		return;
	}
	SelectedNotifyIndex = Index;
	const FString Name = Notifies[Index].NotifyName.ToString();
	std::fill(NotifyNameBuffer, NotifyNameBuffer + sizeof(NotifyNameBuffer), '\0');
	const size_t CopyLength = std::min(Name.size(), sizeof(NotifyNameBuffer) - 1);
	std::copy_n(Name.data(), CopyLength, NotifyNameBuffer);
}

void FAnimationSequenceViewerTab::DrawBlendSpaceControls(
	USkeletalMeshComponent* Component)
{
	ImGui::Separator();
	ImGui::TextUnformatted("BlendSpace 1D");

	bool bSamplesChanged = false;

	// 세 애니메이션을 선택하는 UI
	auto DrawAnimationCombo =
		[this, &bSamplesChanged](const char* Label, UAnimSequence*& Selected)
		{
			const FString CurrentName = Selected
				? Selected->GetPath().substr(SkeletalMesh->GetPath().size() + 2)
				: "None";

			if (ImGui::BeginCombo(Label, CurrentName.c_str()))
			{
				for (UAnimSequence* Candidate : BlendSpaceAnimations)
				{
					const FString Name =
						Candidate->GetPath().substr(
							SkeletalMesh->GetPath().size() + 2
						);

					const bool bSelected = Selected == Candidate;

					if (ImGui::Selectable(Name.c_str(), bSelected))
					{
						Selected = Candidate;
						bSamplesChanged = true;
					}

					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
		};

	DrawAnimationCombo("Idle Animation", IdleAnimation);
	DrawAnimationCombo("Walk Animation", WalkAnimation);
	DrawAnimationCombo("Run Animation", RunAnimation);

	// Threshold 설정
	bSamplesChanged |= ImGui::DragFloat(
		"Idle Threshold",
		&IdleSpeed,
		1.0f,
		0.0f,
		1000.0f
	);

	bSamplesChanged |= ImGui::DragFloat(
		"Walk Threshold",
		&WalkSpeed,
		1.0f,
		0.0f,
		1000.0f
	);

	bSamplesChanged |= ImGui::DragFloat(
		"Run Threshold",
		&RunSpeed,
		1.0f,
		0.0f,
		1000.0f
	);

	const bool bValidSamples =
		IdleAnimation &&
		WalkAnimation &&
		RunAnimation &&
		IdleSpeed < WalkSpeed &&
		WalkSpeed < RunSpeed;

	if (bValidSamples && bSamplesChanged)
	{
		RebuildPreviewBlendSpace();

		if (bUseBlendSpace)
		{
			Component->PlayBlendSpace(
				PreviewBlendSpace,
				BlendInput
			);

			Component->SetAnimationPlayRate(
				SpeedValues[PlaybackSpeedIndex]
			);

			Component->SetAnimationLooping(bLooping);
		}
	}

	if (!bValidSamples)
	{
		ImGui::TextDisabled(
			"Use three valid samples with increasing thresholds."
		);
	}

	ImGui::BeginDisabled(!bValidSamples);

	if (ImGui::Checkbox(
		"Enable BlendSpace",
		&bUseBlendSpace))
	{
		if (bUseBlendSpace)
		{
			bUseTwoWayBlend = false;

			Component->PlayBlendSpace(
				PreviewBlendSpace,
				BlendInput
			);

			Component->SetAnimationPlayRate(
				SpeedValues[PlaybackSpeedIndex]
			);

			Component->SetAnimationLooping(bLooping);
		}
		else
		{
			Component->PlayAnimation(Animation);
		}
	}

	ImGui::EndDisabled();

	if (bUseBlendSpace && bValidSamples)
	{
		if (ImGui::SliderFloat(
			"Speed Input",
			&BlendInput,
			IdleSpeed,
			RunSpeed,
			"%.1f"))
		{
			Component->SetAnimationBlendInput(BlendInput);
		}
	}
}

bool FAnimationSequenceViewerTab::RebuildPreviewBlendSpace()
{
	if (!PreviewBlendSpace || !IdleAnimation || !WalkAnimation || !RunAnimation)
	{
		return false;
	}
	if (!(IdleSpeed < WalkSpeed && WalkSpeed < RunSpeed))
	{
		return false;
	}

	PreviewBlendSpace->ClearSamples();

	const bool bIdleAdded = PreviewBlendSpace->AddSample(IdleSpeed, IdleAnimation);

	const bool bWalkAdded = PreviewBlendSpace->AddSample(WalkSpeed, WalkAnimation);

	const bool bRunAdded = PreviewBlendSpace->AddSample(RunSpeed, RunAnimation);

	return bIdleAdded && bWalkAdded && bRunAdded;
}
