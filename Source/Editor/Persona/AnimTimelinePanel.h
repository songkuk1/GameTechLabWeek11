#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include <functional>

class FAnimTimelinePanel : public IEditorPanel
{
public:
	FAnimTimelinePanel(UAnimSingleNodeInstance* InInstance, UAnimSequence* InSequence, FString InPanelName);
	virtual ~FAnimTimelinePanel() override = default;

	virtual bool Init() override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual void OnRender() override;

	virtual const char* GetPanelName() const override { return PanelName.c_str(); }

	void SetTarget(UAnimSingleNodeInstance* InInstance, UAnimSequence* InSequence);

	void SetNotifySelectedCallback(std::function<void(int32)> InCallback) { OnNotifySelected = std::move(InCallback); }
	int32 GetSelectedNotifyIndex() const { return SelectedNotifyIndex; }
	void SetSelectedNotifyIndex(int32 InIndex) { SelectedNotifyIndex = InIndex; }

private:
	void LoadIcons();
	void DrawTransportControls();
	void DrawTimeline();

	UAnimSingleNodeInstance* SingleNodeInstance = nullptr;
	UAnimSequence* AnimSequence = nullptr;
	FString PanelName;

	std::function<void(int32)> OnNotifySelected;

	// 노티파이 우클릭/추가 팝업 상태
	int32 SelectedNotifyIndex = -1;
	float PendingNotifyTime = 0.0f;
	char NewNotifyNameBuffer[128] = "HitCheck";
	bool bOpenAddNotifyPopup = false;

	class UTexture2D* IconToFront = nullptr;
	class UTexture2D* IconStepBack = nullptr;
	class UTexture2D* IconBackward = nullptr;
	class UTexture2D* IconRecord = nullptr;
	class UTexture2D* IconForward = nullptr;
	class UTexture2D* IconPause = nullptr;
	class UTexture2D* IconStepForward = nullptr;
	class UTexture2D* IconToEnd = nullptr;
	class UTexture2D* IconLoop = nullptr;
	bool bIconsLoaded = false;
};
