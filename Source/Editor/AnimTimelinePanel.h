#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"

class FAnimSequencePreviewTab;

class FAnimTimelinePanel : public IEditorPanel
{
public:
	FAnimTimelinePanel(FAnimSequencePreviewTab& InViewer, FString InPanelName);
	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	// 이 패널을 소유한 탭. 재생 상태와 시간 진행은 Viewer가 관리한다.
	FAnimSequencePreviewTab& Viewer;
	FString PanelName;
};
