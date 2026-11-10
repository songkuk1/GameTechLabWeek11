#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"
#include <vector>

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
	void DrawSequencer();
	// 이 패널을 소유한 탭. 재생 상태와 시간 진행은 Viewer가 관리한다.
	FAnimSequencePreviewTab& Viewer;
	FString PanelName;
	int32_t SelectedNotify = -1;
	int32_t DragNotify = -1;
	int32_t DragStartFrame = 0;
	float DragStartMouseX = 0.0f;
	float DragPixelsPerFrame = 1.0f;
	int32_t ContextFrame = 0;
	float ContextTime = 0.0f;
	int32_t ContextNotify = -1;
	std::vector<char> EditName;
	float EditTriggerTime = 0.0f;
	float EditDuration = 0.0f;
};
