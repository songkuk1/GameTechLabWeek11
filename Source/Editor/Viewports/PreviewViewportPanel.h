#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/Viewport.h"

#include <functional>

class FEditorViewportClient;
class UWorld;

class FPreviewViewportPanel : public IEditorPanel
{
public:
	// InPanelName은 ImGui 창 이름이라 탭마다 달라야 한다. "Viewport##탭이름"처럼 넘긴다
	FPreviewViewportPanel(UWorld* InWorld, FString InPanelName, std::function<void()> InDrawToolbar);
	~FPreviewViewportPanel() = default;

	FPreviewViewportPanel(const FPreviewViewportPanel&) = delete;
	FPreviewViewportPanel& operator=(const FPreviewViewportPanel&) = delete;

	bool Init() override;
	// 지난 OnRender에서 잰 창 크기를 렌더 전에 Viewport에 반영한다
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

	// 우클릭 캡처와 Hover를 채워 카메라 입력을 전달한다. 엔진이 넘긴 BaseInput에는 속도·감도·활성 여부가 들어 있다
	void RouteInput(FViewportInput BaseInput);

	// 지난 OnRender 기준 마우스 위치를 Viewport 출력 픽셀 좌표로 바꾼다. 이미지가 없으면 false
	bool GetMouseViewportPosition(FVector2& OutPosition) const;
	bool IsHovered() const { return bHovered; }
	// 우클릭 카메라 조작 중이면 true. 이때는 Gizmo를 건드리지 않는다
	bool IsCameraCaptured() const { return bCaptured; }

	FViewport* GetViewport() const { return Viewport.get(); }
	FEditorViewportClient* GetEditorClient() const { return EditorClient; }

private:
	UWorld* World = nullptr; // 탭 소유
	FString PanelName;
	std::function<void()> DrawToolbar;

	TUniquePtr<FViewport> Viewport;
	FEditorViewportClient* EditorClient = nullptr; // Viewport 소유

	// 창이 가려지면 0이 되어 엔진이 렌더를 건너뛴다
	FVector2 RequestedSize{};
	// 지난 OnRender에서 그린 이미지의 화면 좌표 사각형. 크기가 0이면 이미지가 없었다
	ImVec2 ImageMin{};
	ImVec2 ImageSize{};
	// 지난 OnRender 기준. 입력은 ImGui 프레임보다 먼저 처리된다
	bool bHovered = false;
	// 우클릭을 누른 채 창 밖으로 나가도 놓을 때까지 카메라를 잡고 있는다
	bool bCaptured = false;
};