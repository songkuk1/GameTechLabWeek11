#include "EnginePCH.h"
#include "AnimTimelinePanel.h"
#include "Asset/AssetManager.h"
#include "Render/Texture2D.h"
#include "imgui_neo_sequencer.h"
#include <algorithm>
#include <cmath>

FAnimTimelinePanel::FAnimTimelinePanel(UAnimSingleNodeInstance* InInstance, UAnimSequence* InSequence, FString InPanelName)
	: SingleNodeInstance(InInstance), AnimSequence(InSequence), PanelName(std::move(InPanelName))
{
}

void FAnimTimelinePanel::SetTarget(UAnimSingleNodeInstance* InInstance, UAnimSequence* InSequence)
{
	SingleNodeInstance = InInstance;
	AnimSequence = InSequence;
	SelectedNotifyIndex = -1;
	if (OnNotifySelected) OnNotifySelected(-1);
}

void FAnimTimelinePanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
}

void FAnimTimelinePanel::OnRender()
{
	if (!ImGui::Begin(PanelName.c_str()))
	{
		ImGui::End();
		return;
	}

	if (!SingleNodeInstance || !AnimSequence)
	{
		ImGui::TextDisabled("No animation loaded.");
		ImGui::End();
		return;
	}

	// 1. 상단 트랜스포트 바
	DrawTransportControls();

	ImGui::Separator();

	// 2. 하단 시퀀서 (타임라인 눈금자 + 스크러버 + Notifies 트랙)
	DrawTimeline();

	ImGui::End();
}

void FAnimTimelinePanel::DrawTransportControls()
{
	LoadIcons();
	const float Duration = AnimSequence->GetPlayLength();
	const float FrameRate = AnimSequence->GetFrameRate() > 0.0f ? AnimSequence->GetFrameRate() : 30.0f;
	const int32 TotalFrames = AnimSequence->GetTotalFrames() > 0 ? AnimSequence->GetTotalFrames() : static_cast<int32>(Duration * FrameRate);
	const float CurrentTime = SingleNodeInstance->GetPosition();
	const int32 CurrentFrame = static_cast<int32>(std::round(CurrentTime * FrameRate));
	const ImVec2 BtnSize(18.0f, 18.0f);

	auto DrawIconButton = [](const char* Id, UTexture2D* Icon, const char* FallbackText, const ImVec2& Size, const ImVec4& Tint = ImVec4(1, 1, 1, 1)) -> bool
		{
			if (Icon && Icon->GetResource())
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.2f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.35f));
				bool bClicked = ImGui::ImageButton(Id, (ImTextureID)Icon->GetResource()->GetRawSRV(), Size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), Tint);
				ImGui::PopStyleColor(3);
				return bClicked;
			}
			return ImGui::Button(FallbackText);
		};

	// 1. |< (처음으로)
	if (DrawIconButton("##ToFront", IconToFront, "|<", BtnSize))
	{
		SingleNodeInstance->Pause();
		SingleNodeInstance->SetPosition(0.0f);
	}
	ImGui::SetItemTooltip("To Beginning (First Frame)");

	// 2. << (1프레임 뒤로)
	ImGui::SameLine();
	if (DrawIconButton("##StepBack", IconStepBack, "<<", BtnSize))
	{
		SingleNodeInstance->StepBackward(1.0f / FrameRate);
	}
	ImGui::SetItemTooltip("Previous Frame (Step Back)");

	// 3. < (역방향 재생 / 일시정지 토글)
	ImGui::SameLine();
	const bool bPlayingReverse = SingleNodeInstance->IsPlaying() && SingleNodeInstance->IsReverse();
	UTexture2D* RevIcon = bPlayingReverse ? IconPause : IconBackward;
	const char* RevText = bPlayingReverse ? "||" : "<";
	if (DrawIconButton("##ReversePlay", RevIcon, RevText, BtnSize))
	{
		if (bPlayingReverse) SingleNodeInstance->Pause();
		else SingleNodeInstance->PlayReverse();
	}
	ImGui::SetItemTooltip(bPlayingReverse ? "Pause" : "Play Reverse");

	// 4. ● (녹화 / 현재 프레임에 노티파이 키 추가)
	ImGui::SameLine();
	if (DrawIconButton("##Record", IconRecord, "O", BtnSize, ImVec4(0.85f, 0.2f, 0.2f, 1.0f)))
	{
		PendingNotifyTime = SingleNodeInstance->GetPosition();
		bOpenAddNotifyPopup = true;
	}
	ImGui::SetItemTooltip("Add Notify at Current Frame");

	// 5. > (순방향 재생 / 일시정지 토글)
	ImGui::SameLine();
	const bool bPlayingForward = SingleNodeInstance->IsPlaying() && !SingleNodeInstance->IsReverse();
	UTexture2D* FwdIcon = bPlayingForward ? IconPause : IconForward;
	const char* FwdText = bPlayingForward ? "||" : ">";
	if (DrawIconButton("##ForwardPlay", FwdIcon, FwdText, BtnSize))
	{
		if (bPlayingForward) SingleNodeInstance->Pause();
		else SingleNodeInstance->Play();
	}
	ImGui::SetItemTooltip(bPlayingForward ? "Pause" : "Play Forward");

	// 6. >> (1프레임 앞으로)
	ImGui::SameLine();
	if (DrawIconButton("##StepFwd", IconStepForward, ">>", BtnSize))
	{
		SingleNodeInstance->StepForward(1.0f / FrameRate);
	}
	ImGui::SetItemTooltip("Next Frame (Step Forward)");

	// 7. >| (마지막으로)
	ImGui::SameLine();
	if (DrawIconButton("##ToEnd", IconToEnd, ">|", BtnSize))
	{
		SingleNodeInstance->Pause();
		SingleNodeInstance->SetPosition(Duration);
	}
	ImGui::SetItemTooltip("To End (Last Frame)");

	// 8. Loop (루프 토글)
	ImGui::SameLine();
	const bool bLooping = SingleNodeInstance->IsLooping();
	ImVec4 LoopTint = bLooping ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f) : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
	if (DrawIconButton("##Loop", IconLoop, "Loop", BtnSize, LoopTint))
	{
		SingleNodeInstance->SetLooping(!bLooping);
	}
	ImGui::SetItemTooltip(bLooping ? "Loop: Enabled" : "Loop: Disabled");

	// 9. 배속 조절
	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();
	float CurrentRate = std::abs(SingleNodeInstance->GetPlayRate());
	ImGui::SetNextItemWidth(65.0f);
	if (ImGui::DragFloat("##Rate", &CurrentRate, 0.05f, 0.1f, 3.0f, "x%.1f"))
	{
		SingleNodeInstance->SetPlayRate(SingleNodeInstance->IsReverse() ? -CurrentRate : CurrentRate);
	}
	ImGui::SetItemTooltip("Playback Rate");

	// 10. 프레임 번호 및 시간 정보
	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();
	ImGui::Text("Frame:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	int32 InputFrame = CurrentFrame;
	if (ImGui::InputInt("##FrameInput", &InputFrame, 0, 0))
	{
		InputFrame = std::clamp(InputFrame, 0, TotalFrames > 0 ? TotalFrames - 1 : 0);
		SingleNodeInstance->Pause();
		SingleNodeInstance->SetPosition(static_cast<float>(InputFrame) / FrameRate);
	}
	ImGui::SameLine();
	ImGui::Text("/ %d  (%.2fs / %.2fs)", TotalFrames, CurrentTime, Duration);
}

void FAnimTimelinePanel::DrawTimeline()
{
	const float Duration = AnimSequence->GetPlayLength();
	const float FrameRate = AnimSequence->GetFrameRate() > 0.0f ? AnimSequence->GetFrameRate() : 30.0f;
	const int32 TotalFrames = AnimSequence->GetTotalFrames() > 0 ? AnimSequence->GetTotalFrames() : static_cast<int32>(Duration * FrameRate);

	float CurrentTime = SingleNodeInstance->GetPosition();
	int32_t CurrentFrame = static_cast<int32_t>(std::round(CurrentTime * FrameRate));
	const int32_t FrameBeforeInput = CurrentFrame;
	int32_t StartFrame = 0;
	int32_t EndFrame = TotalFrames > 0 ? TotalFrames : 1;

	// NeoSequencer 렌더링
	if (ImGui::BeginNeoSequencer("##AnimNeoSequencer", &CurrentFrame, &StartFrame, &EndFrame, ImVec2(0, 0), ImGuiNeoSequencerFlags_None))
	{
		// 노티파이 트랙 렌더링
		if (ImGui::BeginNeoTimelineEx("Notifies"))
		{
			TArray<FAnimNotifyEvent>& Notifies = AnimSequence->GetMutableNotifies();
			int32 NotifyToDelete = -1;

			for (int32 i = 0; i < Notifies.Num(); ++i)
			{
				FAnimNotifyEvent& Notify = Notifies[i];
				int32_t KeyFrame = static_cast<int32_t>(std::round(Notify.TriggerTime * FrameRate));
				const int32_t OriginalFrame = KeyFrame;

				FString DisplayNameStr = Notify.NotifyName.ToString();
				if (DisplayNameStr.empty())
				{
					DisplayNameStr = "Notify";
				}
				const char* DisplayName = DisplayNameStr.c_str();

				char NotifyWidgetId[64];
				snprintf(NotifyWidgetId, sizeof(NotifyWidgetId), "Notify##%d", i);

				bool bHovered = false;
				bool bRightClicked = false;
				const bool bClicked = ImGui::NeoNotify(NotifyWidgetId, &KeyFrame, DisplayName, (SelectedNotifyIndex == i), 0, EndFrame, &bHovered, &bRightClicked);

				// 키프레임을 마우스로 드래그하면 시간 업데이트 및 재생 위치 동기화
				if (KeyFrame != OriginalFrame)
				{
					Notify.TriggerTime = std::clamp(static_cast<float>(KeyFrame) / FrameRate, 0.0f, Duration);
					SingleNodeInstance->Pause();
					SingleNodeInstance->SetPosition(Notify.TriggerTime);
				}

				// 클릭 시 선택
				if (bClicked)
				{
					SelectedNotifyIndex = i;
					if (OnNotifySelected) OnNotifySelected(i);
				}

				// 호버 시 정보 툴팁
				if (bHovered)
				{
					ImGui::SetTooltip("[%s]\nTime: %.3fs (Frame %d)\n(Drag horizontally to move frame)", DisplayName, Notify.TriggerTime, KeyFrame);
				}

				// 마커 우클릭 시 삭제 팝업 열기
				if (bRightClicked)
				{
					SelectedNotifyIndex = i;
					if (OnNotifySelected) OnNotifySelected(i);
					ImGui::OpenPopup("NotifyContextMenu");
				}
			}

			// 노티파이 마커 우클릭 팝업
			if (ImGui::BeginPopup("NotifyContextMenu"))
			{
				if (SelectedNotifyIndex >= 0 && SelectedNotifyIndex < Notifies.Num())
				{
					const FString DisplayNameStr = Notifies[SelectedNotifyIndex].NotifyName.ToString();
					const char* DisplayName = DisplayNameStr.c_str();
					ImGui::TextDisabled("Notify: %s", DisplayName);
					ImGui::Separator();
					if (ImGui::MenuItem("Delete Notify"))
					{
						NotifyToDelete = SelectedNotifyIndex;
					}
				}
				ImGui::EndPopup();
			}

			if (NotifyToDelete >= 0)
			{
				AnimSequence->RemoveNotify(NotifyToDelete);
				SelectedNotifyIndex = -1;
				if (OnNotifySelected) OnNotifySelected(-1);
			}

			// 트랙 빈 영역 우클릭 시 노티파이 추가 팝업 열기
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
			{
				PendingNotifyTime = SingleNodeInstance->GetPosition();
				bOpenAddNotifyPopup = true;
			}

			ImGui::EndNeoTimeLine();
		}

		ImGui::EndNeoSequencer();
	}

	// 사용자가 타임라인 스크러버를 마우스로 직접 움직였을 때 실시간 포즈 반영
	if (CurrentFrame != FrameBeforeInput)
	{
		SingleNodeInstance->Pause();
		SingleNodeInstance->SetPosition(static_cast<float>(CurrentFrame) / FrameRate);
	}

	// 노티파이 추가 팝업
	if (bOpenAddNotifyPopup)
	{
		ImGui::OpenPopup("Add Notify");
		bOpenAddNotifyPopup = false;
	}

	if (ImGui::BeginPopup("Add Notify"))
	{
		ImGui::Text("Add Notify at %.2fs (Frame %d)", PendingNotifyTime, static_cast<int32>(PendingNotifyTime * FrameRate));
		ImGui::Separator();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputText("Name", NewNotifyNameBuffer, sizeof(NewNotifyNameBuffer));

		if (ImGui::Button("Add") || ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			FAnimNotifyEvent NewEvent;
			NewEvent.NotifyName = FName(NewNotifyNameBuffer);
			NewEvent.TriggerTime = PendingNotifyTime;
			AnimSequence->AddNotify(NewEvent);
			SelectedNotifyIndex = AnimSequence->GetNotifies().Num() - 1;
			if (OnNotifySelected) OnNotifySelected(SelectedNotifyIndex);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::Separator();
		ImGui::TextDisabled("Presets:");
		if (ImGui::Selectable("HitCheck (Custom Event)"))
		{
			FAnimNotifyEvent NewEvent;
			NewEvent.NotifyName = FName("HitCheck");
			NewEvent.TriggerTime = PendingNotifyTime;
			AnimSequence->AddNotify(NewEvent);
			SelectedNotifyIndex = AnimSequence->GetNotifies().Num() - 1;
			if (OnNotifySelected) OnNotifySelected(SelectedNotifyIndex);
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::Selectable("PlaySound (Sound Event)"))
		{
			FAnimNotifyEvent NewEvent;
			NewEvent.NotifyName = FName("PlaySound");
			NewEvent.TriggerTime = PendingNotifyTime;
			AnimSequence->AddNotify(NewEvent);
			SelectedNotifyIndex = AnimSequence->GetNotifies().Num() - 1;
			if (OnNotifySelected) OnNotifySelected(SelectedNotifyIndex);
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::Selectable("Footstep (Custom Event)"))
		{
			FAnimNotifyEvent NewEvent;
			NewEvent.NotifyName = FName("Footstep");
			NewEvent.TriggerTime = PendingNotifyTime;
			AnimSequence->AddNotify(NewEvent);
			SelectedNotifyIndex = AnimSequence->GetNotifies().Num() - 1;
			if (OnNotifySelected) OnNotifySelected(SelectedNotifyIndex);
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void FAnimTimelinePanel::LoadIcons()
{
	if (bIconsLoaded) return;
	bIconsLoaded = true;
	auto LoadOr = [](const char* PrimaryPath, const char* FallbackPath = nullptr) -> UTexture2D*
		{
			UTexture2D* Tex = UAssetManager::Get().LoadTexture(PrimaryPath);
			if (!Tex && FallbackPath)
			{
				Tex = UAssetManager::Get().LoadTexture(FallbackPath);
			}
			return Tex;
		};

	IconToFront = LoadOr("Resources/Icons/Timeline/backward_end.png", "Resources/Icons/Timeline/Backwards.png");
	IconStepBack = LoadOr("Resources/Icons/Timeline/backward_step.png", "Resources/Icons/Timeline/Step_Backwards.png");
	IconBackward = LoadOr("Resources/Icons/Timeline/backward.png", "Resources/Icons/Timeline/Backwards.png");
	IconRecord = LoadOr("Resources/Icons/Timeline/Record.png");
	IconForward = LoadOr("Resources/Icons/Timeline/forward.png", "Resources/Icons/Toolbar/Play.png");
	IconPause = LoadOr("Resources/Icons/Timeline/pause.png", "Resources/Icons/Toolbar/Pause.png");
	IconStepForward = LoadOr("Resources/Icons/Timeline/forward_step.png", "Resources/Icons/Timeline/Step_Forward_.png");
	IconToEnd = LoadOr("Resources/Icons/Timeline/forward_end.png", "Resources/Icons/Timeline/Play.png");
	IconLoop = LoadOr("Resources/Icons/Timeline/Loop.png");
}
