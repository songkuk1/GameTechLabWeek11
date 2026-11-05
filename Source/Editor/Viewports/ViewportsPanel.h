#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/ViewportTypes.h"
#include "Editor/Viewports/Viewport.h"
#include "Render/SceneRenderTargets.h"
#include "Render/Texture2D.h"
class FGizmo;

// 네 편집 View를 담는 패널. View마다 FViewport를 소유하고, 배치·View 선택·우클릭 Capture를 맡는다.
// 카메라는 각 Viewport의 Client가, 렌더 큐는 FViewportRenderCollector가 맡는다.
class FViewportsPanel : public IEditorPanel
{
public:
	static constexpr int32 ViewCount = 4;

	FViewportsPanel();
	// 상단 World/Local 버튼이 바꿀 기즈모를 연결한다. 없으면 버튼을 그리지 않는다.
	void SetGizmo(FGizmo* Value) { Gizmo = Value; }
	// 네 View의 렌더 타깃과 UI 제어 상태를 초기화한다.
	bool Init() override;
	// 프레임 입력에서 Splitter Drag와 View별 UI 요청을 수집한다.
	void Tick(float DeltaTime) override;
	// 각 View Texture와 Splitter·Layout·Preset 조작 UI를 ImGui 패널에 그린다.
	void OnRender() override;
	const char* GetPanelName() const override { return "Viewports"; }

	// View 하나의 Viewport와 그 편집 Client.
	FViewport& GetViewport(int32 ViewIndex);
	const FViewport& GetViewport(int32 ViewIndex) const;
	FEditorViewportClient& GetEditorClient(int32 ViewIndex) const;

	// 네 View의 초기 카메라와 프리셋을 구성한다.
	void InitializeDefaultViews();
	// 현재 Single·Quad 레이아웃 모드를 설정한다.
	void SetLayoutMode(ELayoutMode Mode) { LayoutMode = Mode; }
	ELayoutMode GetLayoutMode() const { return LayoutMode; }
	// Single 레이아웃에서 전체 화면으로 표시할 View 인덱스를 설정한다.
	void SetSingleViewIndex(int32 ViewIndex);
	int32 GetSingleViewIndex() const { return SingleViewIndex; }
	// 외부에서 읽은 Split 비율을 Core 허용 범위로 제한해 저장한다.
	void SetSplitRatio(const FSplitRatio& Value);
	const FSplitRatio& GetSplitRatio() const { return SplitRatio; }

	// 레이아웃 Rect를 계산하고 활성 View를 갱신한다.
	void UpdateLayout(FVector2 WindowSize, FVector2 LocalMousePosition);
	// Splitter Drag 픽셀을 Core 비율 계산에 전달해 레이아웃 상태를 갱신한다.
	void ApplySplitterDrag(EDragAxis Axis, float DeltaPixels, FVector2 WindowSize);
	// 우클릭 Capture와 활성 View를 갱신하고, 활성 View의 Client에 카메라 입력을 넘긴다.
	// Capture 중 휠은 줌 대신 InOutMoveSpeed를 바꾼다.
	void UpdateInput(float DeltaTime, FVector2 LocalMousePosition, float& InOutMoveSpeed, float MouseSensitivity);

	// 레이아웃과 Rect 상태를 기준으로 지정 View의 활성 여부를 반환한다.
	bool IsViewActive(int32 ViewIndex) const;
	// 현재 입력을 소비할 활성 View 인덱스를 반환한다.
	int32 GetActiveViewIndex() const { return ActiveViewIndex; }
	// 입력 영역 밖에서도 마지막 편집 대상을 유지하며 Single에서는 확대 View를 반환한다.
	int32 GetEditorViewIndex() const { return LayoutMode == ELayoutMode::Single ? SingleViewIndex : EditorViewIndex; }
	// 속성 창에서 선택한 View를 공통 편집 대상으로 지정한다.
	void SetEditorViewIndex(int32 Index) { if (Index >= 0 && Index < ViewCount) EditorViewIndex = Index; }
	// 우클릭 입력을 Capture 중인 View 인덱스를 반환한다.
	int32 GetCapturedViewIndex() const { return InputState.CapturedViewIndex; }
	// 지정 View의 패널 로컬 화면 Rect를 반환한다.
	const FRect& GetViewRect(int32 ViewIndex) const;
	// 활성 View의 패널 로컬 마우스 좌표를 역투영해 Ray로 반환한다. 마우스가 다른 View 위면 false다.
	bool TryGetActiveViewRay(FVector2 LocalMousePosition, FRay& OutRay) const;

	FVector2 GetContentSize() const { return {ContentSize.x, ContentSize.y}; }
	FVector2 GetLocalMousePosition() const;
	bool IsHovered() const;

	float ConsumeHorizontalDrag();
	float ConsumeVerticalDrag();
	// UI에서 발생한 Layout 변경 요청을 한 번 소비하도록 반환한다.
	bool ConsumeLayoutRequest(ELayoutMode& OutMode, int32& OutSingleViewIndex);
	// UI에서 발생한 View별 Camera Preset 요청을 한 번 소비하도록 반환한다.
	bool ConsumeCameraPresetRequest(int32& OutViewIndex, EMultipleViewportsCameraPreset& OutPreset);
	// 카메라 이동 속도가 바뀌었을 때 해당 View 왼쪽 아래에 잠시 값을 표시한다.
	void ShowCameraSpeed(int32 ViewIndex, float Speed);

private:
	FGizmo* Gizmo = nullptr;
	struct FViewSlot
	{
		// View별 Viewport와 이번 프레임의 활성 상태
		FViewport Viewport;
		bool bActive = false;
	};

	// ViewRects를 바꾼 뒤 각 Viewport에 같은 크기를 넣는다.
	void SyncViewportRects();
	// 콘솔 stat 명령으로 켜진 항목을 해당 View의 좌상단에 겹쳐 그린다.
	void DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const;
	// ShowCameraSpeed로 요청된 속도 표시가 아직 유효하면 그린다.
	void DrawCameraSpeedNotice(ImDrawList* DrawList) const;
	// View 오른쪽 위 카메라 메뉴. 프리셋 선택과 FOV·Near·Far·Transform 편집을 한 드롭다운에 담는다.
	void DrawCameraMenu(int32 ViewIndex, const ImVec2& ViewTopRight);
	// 카메라 메뉴 안의 투영·Transform 편집 칸. 바뀐 값만 편집 Client에 반영한다.
	void DrawCameraProperties(int32 ViewIndex);

	FViewSlot Slots[ViewCount]{};

	// 레이아웃·View 선택 상태
	static constexpr float MinimumSplitRatio = 0.1f;
	// 예전 FViewSet{}의 값 초기화와 같이 Single로 시작하고, InitializeDefaultViews가 Quad로 바꾼다.
	ELayoutMode LayoutMode = ELayoutMode::Single;
	FSplitRatio SplitRatio{};
	FViewInputState InputState{};
	// Core 함수가 읽고 쓰는 네 View의 패널 로컬 Rect. Viewport에도 같은 값을 넣는다.
	FRect ViewRects[ViewCount]{};
	int32 SingleViewIndex = 0;
	int32 ActiveViewIndex = InvalidViewIndex;
	int32 EditorViewIndex = 0;

	ImVec2 ContentOrigin{};
	ImVec2 ContentSize{1.0f, 1.0f};
	bool bHovered = false;
	// View 위에 그린 버튼·콤보의 화면 영역. 이 위의 클릭은 뷰포트 입력(피킹·기즈모)으로 보지 않는다.
	struct FControlRect
	{
		ImVec2 Min;
		ImVec2 Max;
	};
	TArray<FControlRect> ControlRects;
	// 방금 그린 항목의 영역을 ControlRects에 넣는다.
	void RecordLastItemAsControl();
	float PendingHorizontalDrag = 0.0f;
	float PendingVerticalDrag = 0.0f;
	// 누른 뒤 커서가 Splitter 영역을 벗어나도 버튼을 놓을 때까지 드래그를 유지한다.
	bool bDraggingVerticalSplitter = false;
	bool bDraggingHorizontalSplitter = false;
	bool bHasLayoutRequest = false;
	ELayoutMode RequestedLayoutMode = ELayoutMode::QuadSplit;
	int32 RequestedSingleViewIndex = 0;
	int32 PendingCameraPresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset PendingCameraPreset = EMultipleViewportsCameraPreset::Perspective;
	int32 CameraSpeedViewIndex = InvalidViewIndex;
	float CameraSpeedValue = 0.0f;
	double CameraSpeedShownTime = 0.0;
};
