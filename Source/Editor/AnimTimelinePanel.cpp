#include "EnginePCH.h"
#include "AnimTimelinePanel.h"
#include "FAnimSequencePreviewTab.h"

#include <algorithm>
#include <cmath>
#include <limits>

FAnimTimelinePanel::FAnimTimelinePanel(FAnimSequencePreviewTab& InViewer, FString InPanelName)
	: Viewer(InViewer), PanelName(std::move(InPanelName))
{
}

void FAnimTimelinePanel::OnRender()
{
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		DragNotify = -1;
	if (ImGui::Begin(PanelName.c_str()))
	{
		const float PlayLength = Viewer.GetPlayLength();
		ImGui::BeginDisabled(PlayLength <= 0.0f);
		if (ImGui::Button(Viewer.IsPlaying() ? "Pause" : "Play"))
		{
			if (Viewer.IsPlaying()) Viewer.Pause();
			else Viewer.Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) Viewer.Stop();
		ImGui::SameLine();
		ImGui::Text("%.3f / %.3f s", Viewer.GetCurrentTime(), PlayLength);

		ImGui::EndDisabled();

		if (PlayLength <= 0.0f)
			ImGui::TextDisabled("No playable animation");
		else
			DrawSequencer();
	}
	ImGui::End();
}

void FAnimTimelinePanel::DrawSequencer()
{
	UAnimSequence* Sequence = Viewer.GetSequence();
	if (!Sequence)
		return;

	const double FrameRate = Sequence->GetFrameRate();
	const double FrameCount = std::ceil(static_cast<double>(Viewer.GetPlayLength()) * FrameRate);
	if (!std::isfinite(FrameRate) || FrameRate <= 0.0 || !std::isfinite(FrameCount)
		|| FrameCount < 1.0 || FrameCount > static_cast<double>((std::numeric_limits<int32_t>::max)() - 1))
	{
		ImGui::TextDisabled("Invalid animation frame range");
		return;
	}

	int32_t StartFrame = 0;
	int32_t EndFrame = static_cast<int32_t>(FrameCount);
	int32_t CurrentFrame = static_cast<int32_t>(std::clamp(
		std::round(static_cast<double>(Viewer.GetCurrentTime()) * FrameRate), 0.0, FrameCount));
	const int32_t FrameBeforeInput = CurrentFrame;
	bool bAddNotify = false;
	bool bUpdateNotify = false;
	bool bDeleteNotify = false;
	int32_t RightClickedNotify = -1;
	const ImVec2 Size(0.0f, std::max(100.0f, ImGui::GetContentRegionAvail().y));
	if (ImGui::BeginNeoSequencer("AnimationSequence", &CurrentFrame, &StartFrame, &EndFrame, Size))
	{
		// 키프레임 UI는 표시와 선택에 사용하고, 이동량은 LevelSequence처럼 프레임 너비로 계산한다.
		bool bHasNotify = false;
		const auto& Notifies = Sequence->GetNotifyData();
		if (SelectedNotify >= Notifies.Num()) SelectedNotify = -1;
		if (DragNotify >= Notifies.Num()) DragNotify = -1;
		for (int32 Index = 0; Index < Notifies.Num(); ++Index)
		{
			const NotifyData& Notify = Notifies[Index];
			if (!std::isfinite(Notify.Duration) || Notify.Duration < 0.0f || !std::isfinite(Notify.TriggerTime)
				|| Notify.TriggerTime < 0.0f || Notify.TriggerTime > Viewer.GetPlayLength())
				continue;

			bHasNotify = true;
			ImGui::PushID(Index);
			const FString Name = Notify.NotifyName.empty() ? "Notify" : Notify.NotifyName;
			const FString RowLabel = Name + "###NotifyRow";
			if (ImGui::BeginNeoTimelineEx(RowLabel.c_str()))
			{
				int32_t NotifyFrame = static_cast<int32_t>(std::clamp(
					std::round(static_cast<double>(Notify.TriggerTime) * FrameRate), 0.0, FrameCount));
				if (DragNotify == Index && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					const double Offset = (ImGui::GetMousePos().x - DragStartMouseX) / DragPixelsPerFrame;
					const int32_t TargetFrame = static_cast<int32_t>(std::clamp(
						std::round(static_cast<double>(DragStartFrame) + Offset), 0.0, FrameCount));
					if (TargetFrame != NotifyFrame)
					{
						const float TargetTime = TargetFrame == EndFrame ? Viewer.GetPlayLength()
							: static_cast<float>(static_cast<double>(TargetFrame) / FrameRate);
						const float NewTime = std::min(TargetTime, std::max(0.0f, Viewer.GetPlayLength() - Notify.Duration));
						if (Sequence->SetNotifyTriggerTime(static_cast<uint32>(Index), NewTime))
						{
							NotifyFrame = static_cast<int32_t>(std::clamp(std::round(static_cast<double>(NewTime) * FrameRate), 0.0, FrameCount));
							Viewer.SetCurrentTime(NewTime);
						}
					}
				}

				const bool bSelected = SelectedNotify == Index;
				if (Notify.Duration > 0.0f)
				{
					const int32_t DurationEndFrame = static_cast<int32_t>(std::clamp(
						std::ceil((static_cast<double>(Notify.TriggerTime) + Notify.Duration) * FrameRate), 0.0, FrameCount));
					ImGui::NeoDrawTimelineRect(NotifyFrame, DurationEndFrame, ImVec4(0.3f, 0.6f, 0.9f, 0.3f));
				}
				if (bSelected)
					ImGui::PushNeoSequencerStyleColor(ImGuiNeoSequencerCol_Keyframe, IM_COL32(255, 190, 60, 255));
				ImGui::NeoKeyframe(&NotifyFrame);
				const bool bLabelHovered = ImGui::NeoDrawKeyframeLabel(NotifyFrame, Name.c_str());
				if (ImGui::IsNeoKeyframeHovered() || bLabelHovered)
				{
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						SelectedNotify = Index;
						DragNotify = Index;
						DragStartFrame = NotifyFrame;
						DragStartMouseX = ImGui::GetMousePos().x;
						DragPixelsPerFrame = std::max(ImGui::NeoGetFrameWidth(), 0.001f);
						Viewer.Pause();
					}
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
						RightClickedNotify = Index;
					ImGui::SetTooltip("%s | %.3f s | duration %.3f s | frame %d", Name.c_str(), Notify.TriggerTime, Notify.Duration, NotifyFrame);
				}
				if (bSelected)
					ImGui::PopNeoSequencerStyleColor();
				ImGui::EndNeoTimeLine();
			}
			ImGui::PopID();
		}
		if (!bHasNotify && ImGui::BeginNeoTimelineEx("Notifies"))
			ImGui::EndNeoTimeLine();

		int32_t MouseFrame = 0;
		if (DragNotify < 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
			&& (RightClickedNotify >= 0 || ImGui::NeoGetMouseFrame(&MouseFrame)))
		{
			ContextNotify = RightClickedNotify;
			if (ContextNotify >= 0)
			{
				const NotifyData& Notify = Notifies[ContextNotify];
				SelectedNotify = ContextNotify;
				EditName.assign(std::max(size_t(1024), Notify.NotifyName.size() + 256), '\0');
				std::copy(Notify.NotifyName.begin(), Notify.NotifyName.end(), EditName.begin());
				EditTriggerTime = Notify.TriggerTime;
				EditDuration = Notify.Duration;
				Viewer.Pause();
			}
			else
			{
				// 메뉴를 연 위치를 보관한다. 메뉴 안에서 마우스를 움직여도 추가 시간은 유지한다.
				ContextFrame = MouseFrame;
				ContextTime = MouseFrame >= EndFrame ? Viewer.GetPlayLength()
					: static_cast<float>(static_cast<double>(MouseFrame) / FrameRate);
			}
			ImGui::OpenPopup("NotifyContextMenu");
		}
		if (ImGui::BeginPopup("NotifyContextMenu"))
		{
			if (ContextNotify < 0)
			{
				ImGui::TextDisabled("Frame %d | %.3f s", ContextFrame, ContextTime);
				ImGui::Separator();
				bAddNotify = ImGui::MenuItem("Add Notify");
			}
			else if (Notifies.IsValidIndex(ContextNotify))
			{
				ImGui::TextUnformatted("Notify Properties");
				ImGui::Separator();
				ImGui::InputText("Name", EditName.data(), EditName.size());
				ImGui::InputFloat("Trigger Time (s)", &EditTriggerTime, 0.0f, 0.0f, "%.3f");
				ImGui::InputFloat("Duration (s)", &EditDuration, 0.0f, 0.0f, "%.3f");
				const bool bValid = EditName[0] != '\0' && std::isfinite(EditTriggerTime) && std::isfinite(EditDuration)
					&& EditTriggerTime >= 0.0f && EditTriggerTime <= Viewer.GetPlayLength()
					&& EditDuration >= 0.0f && EditDuration <= Viewer.GetPlayLength() - EditTriggerTime;
				if (!bValid)
					ImGui::TextDisabled("Enter a name and a time range within the clip.");
				ImGui::BeginDisabled(!bValid);
				if (ImGui::Button("Apply"))
				{
					bUpdateNotify = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
				ImGui::Separator();
				bDeleteNotify = ImGui::MenuItem("Delete Notify");
			}
			else
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		ImGui::EndNeoSequencer();
	}

	// 배열 변경과 속성 적용은 순회 및 Neo Sequencer가 끝난 뒤 처리한다.
	if (bDeleteNotify)
	{
		if (Sequence->RemoveNotify(static_cast<uint32>(ContextNotify)))
		{
			SelectedNotify = -1;
			DragNotify = -1;
			ContextNotify = -1;
		}
		return;
	}
	if (bUpdateNotify)
	{
		if (Sequence->UpdateNotify(static_cast<uint32>(ContextNotify), NotifyData{ EditName.data(), EditTriggerTime, EditDuration }))
			Viewer.SetCurrentTime(EditTriggerTime);
		return;
	}
	if (bAddNotify)
	{
		SelectedNotify = static_cast<int32_t>(Sequence->AddNotify(NotifyData{ "Notify", ContextTime, 0.0f }));
		DragNotify = -1;
		Viewer.SetCurrentTime(ContextTime);
		return;
	}

	// 재생 중 표시를 프레임에 맞추되, 사용자가 재생 위치를 바꿨을 때만 초로 환산한다.
	if (CurrentFrame != FrameBeforeInput)
	{
		const float Time = CurrentFrame >= EndFrame ? Viewer.GetPlayLength()
			: static_cast<float>(static_cast<double>(CurrentFrame) / FrameRate);
		Viewer.SetCurrentTime(Time);
	}
}
