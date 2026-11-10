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

#include "Serialization/JsonArchive.h"

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
	-1.0f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f
};

FAnimationSequenceViewerTab::FAnimationSequenceViewerTab(USkeletalMesh* InSkeletalMesh, UAnimSequence* InAnimation) : FEditorTab(MakeAnimationSequenceViewerSettings(InAnimation)), SkeletalMesh(InSkeletalMesh), Animation(InAnimation)
{
	PreviewScene = MakeUnique<FPreviewScene>();
	PreviewActor = Cast<ASkeletalMeshActor>(PreviewScene->GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));
	InitialPreviewActorTransform = PreviewActor->GetActorTransform();
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

	PreviewBlendSpace = NewObject<UBlendSpace1D>();

	const bool bSkeletonSet = PreviewBlendSpace->SetSkeleton(SkeletalMesh->GetSkeleton());

	assert(bSkeletonSet);

	const bool bInitialSampleAdded = PreviewBlendSpace->AddSample( PreviewBlendSpace->GetAxis().MinValue, Animation);

	assert(bInitialSampleAdded);

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
	USkeletalMeshComponent* Component = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;
	if (!Component || !Animation)
	{
		return;
	}

	static const char* PreviewModeLabels[] =
	{
		"Sequence",
		"Two-Way Blend",
		"BlendSpace 1D"
	};

	const int32 PreviewModeIndex = std::clamp(static_cast<int32>(PreviewMode), 0, IM_ARRAYSIZE(PreviewModeLabels) - 1);

	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::BeginCombo("Preview Mode", PreviewModeLabels[PreviewModeIndex]))
	{
		for (int32 Index = 0; Index < IM_ARRAYSIZE(PreviewModeLabels); ++Index)
		{
			const EAnimationPreviewMode CandidateMode = static_cast<EAnimationPreviewMode>(Index);
			const bool bSelected = PreviewMode == CandidateMode;
			const bool bAvailable = Index == 0 || (Index == 1 && BlendAnimation) || (Index == 2 && PreviewBlendSpace && !PreviewBlendSpace->GetSamples().IsEmpty());

			ImGui::BeginDisabled(!bAvailable);
			if (ImGui::Selectable(PreviewModeLabels[Index], bSelected))
			{
				SetPreviewMode(CandidateMode, Component);
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
			ImGui::EndDisabled();
		}

		ImGui::EndCombo();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Playback");
	ImGui::SameLine();

	const float Duration = Component->GetAnimationPlayLength();
	const float FrameRate = Component->GetAnimationFrameRate();

	if (ImGui::Button(bPlaying ? "Pause" : "Play"))
	{
		if (!bPlaying && !bLooping && Duration > 0.0f)
		{
			const float CurrentTime = Component->GetAnimationPlaybackTime();
			const float PlayRate = Component->GetAnimationPlayRate();

			if (PlayRate > 0.0f && CurrentTime >= Duration)
			{
				Component->SetAnimationPlaybackTime(0.0f);
			}
			else if (PlayRate < 0.0f && CurrentTime <= 0.0f)
			{
				Component->SetAnimationPlaybackTime(Duration);
			}
		}

		bPlaying = !bPlaying;
	}

	ImGui::SameLine();

	if (ImGui::Button("Restart"))
	{
		PreviewActor->SetActorTransform(InitialPreviewActorTransform);
		Component->SetAnimationPlaybackTime(Component->GetAnimationPlayRate() < 0.0f ? Duration : 0.0f);
		Component->TickAnimation(0.0f);
		bPlaying = true;
	}

	ImGui::SameLine();

	ImGui::BeginDisabled(FrameRate <= 0.0f);

	if (ImGui::Button("< Frame"))
	{
		bPlaying = false;
		Component->SetAnimationPlaybackTime(Component->GetAnimationPlaybackTime() - (1.0f / FrameRate));
	}

	ImGui::SameLine();

	if (ImGui::Button("Frame >"))
	{
		bPlaying = false;
		Component->SetAnimationPlaybackTime(Component->GetAnimationPlaybackTime() + (1.0f / FrameRate));
	}

	ImGui::EndDisabled();

	ImGui::SameLine();

	static const char* SpeedLabels[] = { "Reverse 1.0x", "0.25x", "0.5x", "1.0x", "1.5x", "2.0x" };

	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::BeginCombo("Speed", SpeedLabels[PlaybackSpeedIndex]))
	{
		for (int32 Index = 0; Index < IM_ARRAYSIZE(SpeedLabels); ++Index)
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

	ImGui::SameLine();

	if (ImGui::Checkbox("Loop", &bLooping))
	{
		Component->SetAnimationLooping(bLooping);
	}

	ImGui::SameLine();

	bool bRootMotionEnabled = Component->IsRootMotionEnabled();

	if (ImGui::Checkbox("Root Motion", &bRootMotionEnabled))
	{
		Component->SetRootMotionEnabled(bRootMotionEnabled);
		PreviewActor->SetActorTransform(InitialPreviewActorTransform);
		Component->SetAnimationPlaybackTime(0.0f);
		Component->TickAnimation(0.0f);
		bPlaying = false;
	}

	const float CurrentTime = Component->GetAnimationPlaybackTime();
	const UAnimDataModel* DataModel = Animation->GetDataModel();
	const int32 TotalFrames = DataModel ? std::max(1, DataModel->NumberOfFrames) : 1;

	if (PreviewMode == EAnimationPreviewMode::BlendSpace)
	{
		ImGui::Text("Time: %.3f / %.3f sec", CurrentTime, Duration);
	}
	else
	{
		int32 CurrentFrame = 0;

		if (FrameRate > 0.0f)
		{
			CurrentFrame = std::clamp(static_cast<int32>(std::floor(CurrentTime * FrameRate)), 0, TotalFrames - 1);

			if (Duration > 0.0f && CurrentTime >= Duration - 1.0e-6f)
			{
				CurrentFrame = TotalFrames - 1;
			}
		}

		const char* FrameLabel = PreviewMode == EAnimationPreviewMode::TwoWayBlend ? "Source A Frame" : "Frame";
		ImGui::Text("%s %d / %d     %.3f / %.3f sec", FrameLabel, CurrentFrame + 1, TotalFrames, CurrentTime, Duration);
	}

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

	if (ImGui::BeginTabBar("AnimationSequenceViewerTabs"))
	{
		if (ImGui::BeginTabItem("Setup"))
		{
			ImGui::Text("Source Animation: %s", Animation->GetPath().c_str());

			if (PreviewMode == EAnimationPreviewMode::TwoWayBlend)
			{
				ImGui::Separator();
				ImGui::TextUnformatted("Two-Way Blend");

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
								SetPreviewMode(EAnimationPreviewMode::TwoWayBlend, Component);
							}

							if (bSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}

						ImGui::EndCombo();
					}

					if (ImGui::SliderFloat("Blend Alpha", &BlendAlpha, 0.0f, 1.0f, "%.2f"))
					{
						Component->SetAnimationBlendAlpha(BlendAlpha);
					}
				}
				else
				{
					ImGui::TextDisabled("No compatible Source B animation.");
				}
			}

			const ImGuiTreeNodeFlags BlendSpaceFlags = PreviewMode == EAnimationPreviewMode::BlendSpace ? ImGuiTreeNodeFlags_DefaultOpen : 0;

			if (ImGui::CollapsingHeader("BlendSpace Setup", BlendSpaceFlags))
			{
				DrawBlendSpaceControls(Component);
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Notifies"))
		{
			ImGui::Text("Editing: %s", Animation->GetPath().c_str());

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

			ImGui::SameLine();

			if (ImGui::Button("Save Notifies"))
			{
				const FString NotifyPath = FJsonArchive::GetAnimationNotifyPath(Animation->GetPath());

				if (FJsonArchive::SaveAnimNotifies(Animation, NotifyPath))
				{
					HTR_LOG(Info, "Saved animation notifies: {}", NotifyPath);
				}
				else
				{
					HTR_LOG(Error, "Failed to save animation notifies: {}", NotifyPath);
				}
			}

			DrawNotifyTrack(Duration);
			DrawNotifyDetails(Duration);

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Curves"))
		{
			DrawCurveEditor(Component, Duration, Component->GetAnimationPlaybackTime());
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
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

	}

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

void FAnimationSequenceViewerTab::DrawCurveEditor(USkeletalMeshComponent* Component, float Duration, float CurrentTime)
{
	if (!Animation || !Animation->GetDataModel())
	{
		return;
	}

	const TArray<FAnimationCurve>& Curves = Animation->GetDataModel()->GetCurveData().Curves;

	ImGui::Separator();

	if (!ImGui::CollapsingHeader("Animation Curves", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (Curves.IsEmpty())
	{
		ImGui::TextDisabled("No animation curves imported.");
		return;
	}

	if (!Curves.IsValidIndex(SelectedCurveIndex))
	{
		SelectedCurveIndex = 0;
	}


	const FAnimationCurve& CurrentSelectedCurve = Curves[SelectedCurveIndex];
	FString SelectedLabel = CurrentSelectedCurve.Name.ToString() + " [" + CurrentSelectedCurve.LayerName.ToString() + "]";

	ImGui::SetNextItemWidth(-1.0f);

	if (ImGui::BeginCombo("Curve", SelectedLabel.c_str()))
	{
		for (int32 Index = 0; Index < Curves.Num(); ++Index)
		{
			const FAnimationCurve& Curve = Curves[Index];
			const FString Label = Curve.Name.ToString() + " [" + Curve.LayerName.ToString() + "]";
			const bool bSelected = SelectedCurveIndex == Index;

			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				SelectedCurveIndex = Index;
			}

			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	const FAnimationCurve& SelectedCurve = Curves[SelectedCurveIndex];
	UAnimInstance* AnimInstance = Component ? Component->GetAnimInstance() : nullptr;
	float RuntimeValue = 0.0f;
	const bool bHasRuntimeValue = AnimInstance && AnimInstance->GetCurveValue(SelectedCurve.Name, SelectedCurve.LayerName, RuntimeValue);

	ImGui::Text("Node Index: %d | Keys: %d", SelectedCurve.NodeIndex, SelectedCurve.Keys.Num());
	ImGui::Text("Source Value: %.4f", SelectedCurve.Evaluate(CurrentTime));

	if (bHasRuntimeValue)
	{
		ImGui::Text("Runtime Value: %.4f", RuntimeValue);
	}
	else
	{
		ImGui::TextDisabled("Runtime Value: Unavailable");
	}

	if (Duration <= 0.0f || SelectedCurve.Keys.IsEmpty())
	{
		ImGui::TextDisabled("Curve has no valid display range.");
		return;
	}

	float MinValue = SelectedCurve.Keys[0].Value;
	float MaxValue = SelectedCurve.Keys[0].Value;

	for (const FAnimationCurveKey& Key : SelectedCurve.Keys)
	{
		MinValue = std::min(MinValue, Key.Value);
		MaxValue = std::max(MaxValue, Key.Value);
	}

	if (bHasRuntimeValue)
	{
		MinValue = std::min(MinValue, RuntimeValue);
		MaxValue = std::max(MaxValue, RuntimeValue);
	}

	if (std::abs(MaxValue - MinValue) < 1.0e-4f)
	{
		MinValue -= 1.0f;
		MaxValue += 1.0f;
	}
	else
	{
		const float Padding = (MaxValue - MinValue) * 0.1f;
		MinValue -= Padding;
		MaxValue += Padding;
	}

	const float PlotHeight = 150.0f;
	const float PlotWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);

	ImGui::InvisibleButton("##AnimationCurveGraph", ImVec2(PlotWidth, PlotHeight));

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();

	const float Left = Min.x + 8.0f;
	const float Right = Max.x - 8.0f;
	const float Top = Min.y + 8.0f;
	const float Bottom = Max.y - 8.0f;

	const float Width = Right - Left;
	const float Height = Bottom - Top;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	DrawList->AddRectFilled(Min, Max, IM_COL32(35, 38, 44, 255), 3.0f);

	for (int32 GridIndex = 0; GridIndex <= 4; ++GridIndex)
	{
		const float Alpha = static_cast<float>(GridIndex) / 4.0f;
		const float X = Left + Width * Alpha;
		const float Y = Top + Height * Alpha;

		DrawList->AddLine(ImVec2(X, Top), ImVec2(X, Bottom), IM_COL32(65, 68, 75, 255));
		DrawList->AddLine(ImVec2(Left, Y), ImVec2(Right, Y), IM_COL32(65, 68, 75, 255));
	}

	auto GetCurvePosition = [&](float Time, float Value)
		{
			const float X = Left + Width * std::clamp(Time / Duration, 0.0f, 1.0f);
			const float ValueAlpha = std::clamp((Value - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f);
			const float Y = Bottom - Height * ValueAlpha;
			return ImVec2(X, Y);
		};

	constexpr int32 SampleCount = 128;
	ImVec2 PreviousPoint = GetCurvePosition(0.0f, SelectedCurve.Evaluate(0.0f));

	for (int32 Index = 1; Index <= SampleCount; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / SampleCount;
		const float Time = Duration * Alpha;
		const float Value = SelectedCurve.Evaluate(Time);
		const ImVec2 CurrentPoint = GetCurvePosition(Time, Value);

		DrawList->AddLine(PreviousPoint, CurrentPoint, IM_COL32(80, 190, 255, 255), 2.0f);
		PreviousPoint = CurrentPoint;
	}

	for (const FAnimationCurveKey& Key : SelectedCurve.Keys)
	{
		DrawList->AddCircleFilled(GetCurvePosition(Key.Time, Key.Value), 4.0f, IM_COL32(255, 190, 70, 255));
	}

	if (bHasRuntimeValue)
	{
		const float ClampedCurrentTime = std::clamp(CurrentTime, 0.0f, Duration);
		const ImVec2 CurrentPoint = GetCurvePosition(ClampedCurrentTime, RuntimeValue);

		DrawList->AddLine(ImVec2(CurrentPoint.x, Top), ImVec2(CurrentPoint.x, Bottom), IM_COL32(255, 100, 100, 200), 1.5f);
		DrawList->AddCircleFilled(CurrentPoint, 5.0f, IM_COL32(255, 100, 100, 255));
	}
}

void FAnimationSequenceViewerTab::ApplyPlaybackSettings(USkeletalMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	Component->SetAnimationPlayRate(SpeedValues[std::clamp(PlaybackSpeedIndex, 0, 4)]);
	Component->SetAnimationLooping(bLooping);
}

bool FAnimationSequenceViewerTab::SetPreviewMode(EAnimationPreviewMode InMode, USkeletalMeshComponent* Component)
{
	if (!Component || !Animation)
	{
		return false;
	}

	switch (InMode)
	{
	case EAnimationPreviewMode::Sequence:
		Component->PlayAnimation(Animation);
		break;

	case EAnimationPreviewMode::TwoWayBlend:
		if (!BlendAnimation)
		{
			return false;
		}
		Component->PlayBlendAnimation(Animation, BlendAnimation, BlendAlpha);
		break;

	case EAnimationPreviewMode::BlendSpace:
		if (!PreviewBlendSpace || PreviewBlendSpace->GetSamples().IsEmpty())
		{
			return false;
		}
		Component->PlayBlendSpace(PreviewBlendSpace, BlendInput);
		break;

	default:
		return false;
	}

	PreviewMode = InMode;
	ApplyPlaybackSettings(Component);
	return true;
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

void FAnimationSequenceViewerTab::DrawBlendSpaceControls(USkeletalMeshComponent* Component)
{
	if (!PreviewBlendSpace || !Component)
	{
		return;
	}
	ImGui::Separator();
	ImGui::TextUnformatted("BlendSpace 1D");
	const FBlendSpaceAxis1D& Axis = PreviewBlendSpace->GetAxis();
	ImGui::Text("Axis: %s", Axis.Name.ToString().c_str());
	ImGui::Text("Range: %.1f ~ %.1f", Axis.MinValue, Axis.MaxValue);
	bool bSamplesChanged = false;
	int32 PendingRemoveIndex = -1;
	int32 PendingValueIndex = -1;
	float PendingSampleValue = 0.0f;
	const float PreviousPlaybackTime = Component->GetAnimationPlaybackTime();
	const TArray<FBlendSample1D>& Samples = PreviewBlendSpace->GetSamples();
	ImGui::Text("Samples: %d", Samples.Num());
	for (int32 Index = 0; Index < Samples.Num(); ++Index)
	{
		const FBlendSample1D& Sample = Samples[Index];
		ImGui::PushID(Index);
		ImGui::Text("Sample %d", Index + 1);
		const FString CurrentName = Sample.Animation ? Sample.Animation->GetPath().substr(SkeletalMesh->GetPath().size() + 2) : "None";
		if (ImGui::BeginCombo("Animation", CurrentName.c_str()))
		{
			for (UAnimSequence* Candidate : BlendSpaceAnimations)
			{
				if (!Candidate)
				{
					continue;
				}
				const FString Name = Candidate->GetPath().substr(SkeletalMesh->GetPath().size() + 2);
				const bool bSelected = Sample.Animation == Candidate;
				if (ImGui::Selectable(Name.c_str(), bSelected))
				{
					if (PreviewBlendSpace->ReplaceSampleAnimation(Index, Candidate))
					{
						bSamplesChanged = true;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		float SampleValue = Sample.SampleValue;
		if (ImGui::DragFloat("Position", &SampleValue, 1.0f, Axis.MinValue, Axis.MaxValue, "%.1f"))
		{
			PendingValueIndex = Index;
			PendingSampleValue = SampleValue;
		}
		float RateScale = Sample.RateScale;
		if (ImGui::DragFloat("Rate Scale", &RateScale, 0.01f, 0.01f, 10.0f, "%.2fx"))
		{
			if (PreviewBlendSpace->SetSampleRateScale(Index, RateScale))
			{
				bSamplesChanged = true;
			}
		}
		if (ImGui::Button("Remove Sample"))
		{
			PendingRemoveIndex = Index;
		}
		ImGui::PopID();
		ImGui::Separator();
	}

	if (PendingRemoveIndex != -1)
	{
		if (PreviewBlendSpace->RemoveSample(PendingRemoveIndex))
		{
			bSamplesChanged = true;
		}
	}
	else if (PendingValueIndex != -1)
	{
		if (PreviewBlendSpace->SetSampleValue(PendingValueIndex, PendingSampleValue))
		{
			bSamplesChanged = true;
		}
	}

	if (ImGui::Button("Add Sample"))
	{
		const TArray<FBlendSample1D>& CurrentSamples = PreviewBlendSpace->GetSamples();

		const float MinValue = Axis.MinValue;
		const float MaxValue = Axis.MaxValue;

		float PreviousValue = MinValue;
		float BestGap = -1.0f;
		float NewSampleValue = (MinValue + MaxValue) * 0.5f;

		for (const FBlendSample1D& Sample : CurrentSamples)
		{
			const float Value = std::clamp(Sample.SampleValue, MinValue, MaxValue);

			const float Gap = Value - PreviousValue;

			if (Gap > BestGap)
			{
				BestGap = Gap;
				NewSampleValue = (PreviousValue + Value) * 0.5f;
			}

			PreviousValue = std::max(PreviousValue, Value);
		}

		const float LastGap = MaxValue - PreviousValue;

		if (LastGap > BestGap)
		{
			BestGap = LastGap;
			NewSampleValue = (PreviousValue + MaxValue) * 0.5f;
		}

		UAnimSequence* DefaultAnimation = Animation;

		if (!DefaultAnimation && !BlendSpaceAnimations.IsEmpty())
		{
			DefaultAnimation = BlendSpaceAnimations[0];
		}

		if (DefaultAnimation && BestGap > 2.0e-4f && PreviewBlendSpace->AddSample(NewSampleValue, DefaultAnimation))
		{
			bSamplesChanged = true;
		}
		else
		{
			ImGui::TextDisabled("No available sample position. Adjust existing samples.");
		}
	}

	if (bSamplesChanged && PreviewMode == EAnimationPreviewMode::BlendSpace)
	{
		if (PreviewBlendSpace->GetSamples().IsEmpty())
		{
			SetPreviewMode(EAnimationPreviewMode::Sequence, Component);
		}
		else
		{
			Component->PlayBlendSpace(PreviewBlendSpace, BlendInput);
			ApplyPlaybackSettings(Component);
			Component->SetAnimationPlaybackTime(PreviousPlaybackTime);
		}
	}

	if (PreviewMode == EAnimationPreviewMode::BlendSpace && !PreviewBlendSpace->GetSamples().IsEmpty())
	{
		if (ImGui::SliderFloat("Blend Input", &BlendInput, Axis.MinValue, Axis.MaxValue, "%.1f"))
		{
			Component->SetAnimationBlendInput(BlendInput);
		}
	}
	else if (PreviewBlendSpace->GetSamples().IsEmpty())
	{
		ImGui::TextDisabled("No samples. Add a sample to use BlendSpace.");
	}
}