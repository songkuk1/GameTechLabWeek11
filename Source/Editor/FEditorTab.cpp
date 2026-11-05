#include "EnginePCH.h"
#include "FEditorTab.h"

FEditorTab::FEditorTab(FEditorTabSettings InSettings) : Settings(InSettings)
{
	if (Settings.DockSpaceName.empty())
		Settings.DockSpaceName = Settings.Title;

	if (Settings.bIsolatePanels)
	{
		WindowClass.ClassId = ImHashStr(Settings.DockSpaceName.c_str());
		WindowClass.DockingAllowUnclassed = false;
	}

}

void FEditorTab::Tick(float DeltaTime)
{
	for(auto& Panel : Panels)
	{
		Panel->Tick(DeltaTime);
	}
}

void FEditorTab::RenderPanels()
{
	for (TUniquePtr<IEditorPanel>& Panel : Panels)
	{
		if (!Panel->IsOpen())
			continue;

		if (Settings.bIsolatePanels)
			//세팅 적용 예약
			ImGui::SetNextWindowClass(&WindowClass);
		// 처음 뜨는 패널만 이 탭에 넣는다. 이후 위치는 imgui.ini가 기억한다
		ImGui::SetNextWindowDockID(DockSpaceId, ImGuiCond_FirstUseEver);
		Panel->OnRender();
	}


}


void FEditorTab::ResolveDockSpaceId()
{
	DockSpaceId = ImGui::GetID(Settings.DockSpaceName.c_str());

}

void FEditorTab::SubmitDockSpace(bool bVisible, ImGuiDockNodeFlags Flags)
{
	//이 ID의 도킹 노드가 아직 없을 때만 새로 만들고, 기본 배치까지 잡아준다. 이후에는 imgui.ini가 기억한다.
	// 기본 배치는 탭이 보여서 영역 크기가 있을 때만 만든다. 막 추가된 탭은 첫 프레임에 아직 선택되지 않아
	// 영역이 0이고, 0 크기는 DockBuilderSetNodeSize가 assert로 막는다.
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	if (bVisible && Avail.x > 0.0f && Avail.y > 0.0f && !ImGui::DockBuilderGetNode(DockSpaceId))
	{
		ImGui::DockBuilderAddNode(DockSpaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(DockSpaceId, Avail);
		BuildDefaultLayout(DockSpaceId);
		ImGui::DockBuilderFinish(DockSpaceId);
	}

	const ImGuiWindowClass* Class = Settings.bIsolatePanels ? &WindowClass : nullptr;
	ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f), bVisible ? Flags : ImGuiDockNodeFlags_KeepAliveOnly, Class);
}

void FEditorTab::BuildDefaultLayout(ImGuiID DockSpaceRoot)
{
	for (TUniquePtr<IEditorPanel>& Panel : Panels)
		ImGui::DockBuilderDockWindow(Panel->GetPanelName(), DockSpaceRoot);

}
