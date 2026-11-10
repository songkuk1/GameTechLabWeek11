#include "EnginePCH.h"
#include "AnimTimelinePanel.h"
#include "FAnimSequencePreviewTab.h"

FAnimTimelinePanel::FAnimTimelinePanel(FAnimSequencePreviewTab& InViewer, FString InPanelName)
	: Viewer(InViewer), PanelName(std::move(InPanelName))
{
}

void FAnimTimelinePanel::OnRender()
{
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

		float Time = Viewer.GetCurrentTime();
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::SliderFloat("##Time", &Time, 0.0f, PlayLength, "%.3f s", ImGuiSliderFlags_AlwaysClamp))
			Viewer.SetCurrentTime(Time);
		ImGui::EndDisabled();

		if (PlayLength <= 0.0f)
			ImGui::TextDisabled("No playable animation");
	}
	ImGui::End();
}
