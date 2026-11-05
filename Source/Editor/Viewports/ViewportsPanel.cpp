#include "EnginePCH.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/ViewportCameraMath.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Camera/CameraTypes.h"
#include "Input/InputSystem.h"

#include "Core/StatOverlay.h"
#include "Core/Stats/LightweightStats.h"
#include "Math/Color.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>

#include "Core/Stats/EditorStats.h"

namespace
{
constexpr ImU32 SplitterColor = IM_COL32(55, 55, 55, 230);
constexpr ImU32 SplitterHoverColor = IM_COL32(255, 192, 0, 255);
constexpr const char* CameraPresetLabels[] = {
	"Perspective", "Ortho (Current)", "Top", "Bottom", "Front", "Back", "Left", "Right"};

// Stat Overlay
constexpr float StatOverlayMargin = 8.0f;
constexpr float StatOverlayPadding = 6.0f;
constexpr ImU32 StatOverlayBackgroundColor = IM_COL32(0, 0, 0, 140);
constexpr ImU32 TitleColor = IM_COL32(255, 210, 60, 255);
constexpr ImU32 ValueColor = IM_COL32(235, 235, 235, 255);

// Camera Speed 표시
constexpr double CameraSpeedNoticeSeconds = 1.5;

// View 상단 컨트롤
constexpr float ViewControlMargin = 8.0f;
constexpr float CameraMenuPopupWidth = 320.0f;
constexpr float CameraPropertyLabelWidth = 120.0f;

// 가장 긴 항목이 잘리지 않는 콤보 폭 (글자 + 좌우 여백 + 펼침 화살표 버튼)
float CalcComboWidth(const char* const* Labels, const int Count)
{
	float MaxTextWidth = 0.0f;
	for (int Index = 0; Index < Count; ++Index)
		MaxTextWidth = std::max(MaxTextWidth, ImGui::CalcTextSize(Labels[Index]).x);
	return MaxTextWidth + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
}

// 네 View Rect의 중앙 경계에서 절반씩 줄여 Splitter가 차지할 실제 빈 공간을 만든다.
void ApplySplitterGutter(const FVector2 WindowSize, FRect ViewRects[4])
{
	const float SplitX = ViewRects[0].Width;
	const float SplitY = ViewRects[0].Height;
	const float HalfThickness = SplitterThickness * 0.5f;
	const float HalfGapX = std::min({HalfThickness, SplitX, WindowSize.X - SplitX});
	const float HalfGapY = std::min({HalfThickness, SplitY, WindowSize.Y - SplitY});
	const float RightX = SplitX + HalfGapX;
	const float BottomY = SplitY + HalfGapY;
	const float LeftWidth = SplitX - HalfGapX;
	const float TopHeight = SplitY - HalfGapY;
	const float RightWidth = WindowSize.X - RightX;
	const float BottomHeight = WindowSize.Y - BottomY;

	ViewRects[0] = {0.0f, 0.0f, LeftWidth, TopHeight};
	ViewRects[1] = {RightX, 0.0f, RightWidth, TopHeight};
	ViewRects[2] = {0.0f, BottomY, LeftWidth, BottomHeight};
	ViewRects[3] = {RightX, BottomY, RightWidth, BottomHeight};
}
}

// 네 View 모두 편집 Client를 가진다. PIE를 이 View에서 실행하면 Game Client가 잠시 대신 그린다.
FViewportsPanel::FViewportsPanel()
{
	for (FViewSlot& Slot : Slots)
		Slot.Viewport.SetEditorClient(MakeUnique<FEditorViewportClient>());
}

FViewport& FViewportsPanel::GetViewport(const int32 ViewIndex)
{
	assert(ViewIndex >= 0 && ViewIndex < ViewCount);
	return Slots[ViewIndex].Viewport;
}

const FViewport& FViewportsPanel::GetViewport(const int32 ViewIndex) const
{
	assert(ViewIndex >= 0 && ViewIndex < ViewCount);
	return Slots[ViewIndex].Viewport;
}

FEditorViewportClient& FViewportsPanel::GetEditorClient(const int32 ViewIndex) const
{
	return *GetViewport(ViewIndex).GetEditorClient();
}

void FViewportsPanel::SyncViewportRects()
{
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
		Slots[ViewIndex].Viewport.SetOutputSize({ViewRects[ViewIndex].Width, ViewRects[ViewIndex].Height});
}

void FViewportsPanel::InitializeDefaultViews()
{
	using namespace ViewportCameraMath;

	const FMinimalViewInfo DefaultView;
	const FCameraProjection Perspective{
		.Mode = EProjectionMode::Perspective,
		.FovDegrees = DefaultView.FOV,
		.OrthoWidth = DefaultView.OrthoWidth,
		.NearClip = DefaultView.NearClip,
		.FarClip = DefaultView.FarClip,
	};

	LayoutMode = ELayoutMode::QuadSplit;
	// 최신 trace의 구도를 사용해 화면 정면은 +X, 화면 오른쪽은 +Y가 되도록 시작한다.
	FViewCamera MainCamera = { {{-11.665390f, 6.117728f, 9.921079f},
		{0.081251f, 0.256328f, -0.291035f, 0.918146f}}, Perspective };
	MainCamera.Transform.Rotation = MakeCameraRotation(
		CameraYawDegrees(MainCamera.Transform.Rotation),
		CameraPitchDegrees(MainCamera.Transform.Rotation));
	GetEditorClient(0).InitializeCamera(MainCamera, EMultipleViewportsCameraPreset::Perspective);

	FCameraProjection Orthographic = Perspective;
	Orthographic.Mode = EProjectionMode::Orthographic;
	GetEditorClient(1).InitializeCamera({ {{0.0f, 0.0f, 10.0f}, {0.0f, 0.70710678f, 0.0f, 0.70710678f}}, Orthographic },
		EMultipleViewportsCameraPreset::Top); // 위쪽에서 -Z 방향을 본다.
	GetEditorClient(2).InitializeCamera({ {{10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}, Orthographic },
		EMultipleViewportsCameraPreset::Front); // +X 쪽에서 -X 방향을 본다.
	GetEditorClient(3).InitializeCamera({ {{0.0f, -10.0f, 0.0f}, {0.0f, 0.0f, 0.70710678f, 0.70710678f}}, Orthographic },
		EMultipleViewportsCameraPreset::Left); // -Y 쪽에서 +Y 방향을 본다.
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
		GetEditorClient(ViewIndex).OnRectChanged(ViewRects[ViewIndex]);
}

// 범위를 검사한 뒤 Single 레이아웃 대상 View를 저장한다.
void FViewportsPanel::SetSingleViewIndex(const int32 ViewIndex)
{
	assert(ViewIndex >= 0 && ViewIndex < ViewCount);
	SingleViewIndex = ViewIndex;
}

// Config에서 받은 비율을 Core 최소 범위로 clamp해 저장한다.
void FViewportsPanel::SetSplitRatio(const FSplitRatio& Value)
{
	SplitRatio = ClampSplitRatio(Value, MinimumSplitRatio);
}

// Core Rect 계산 결과를 Single 대상 슬롯에 재배치하고 활성 View를 갱신한다.
void FViewportsPanel::UpdateLayout(const FVector2 WindowSize, const FVector2 LocalMousePosition)
{
	ComputeViewRects(SplitRatio, WindowSize, ViewRects);
	if (LayoutMode == ELayoutMode::Single)
	{
		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
			ViewRects[ViewIndex] = ViewIndex == SingleViewIndex
				? FRect{0.0f, 0.0f, WindowSize.X, WindowSize.Y}
				: FRect{};
	}
	else
	{
		ApplySplitterGutter(WindowSize, ViewRects);
	}
	SyncViewportRects();
	// 창 크기와 Single/Quad 전환으로 Aspect가 바뀌어도 저장값과 실제 투영값을 함께 보정한다.
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
	{
		if (IsViewRectValid(ViewRects[ViewIndex]))
			GetEditorClient(ViewIndex).OnRectChanged(ViewRects[ViewIndex]);
		// 패널이 이미지를 띄울 Slot. 렌더 타깃은 엔진이 그리기 전에 Viewport에서 맞춘다.
		Slots[ViewIndex].bActive = IsViewActive(ViewIndex);
	}
	// Hover 추적은 비활성화한다. 클릭 위치 판정 함수는 입력 전환에서 계속 사용한다.
	// InputState.HoveredViewIndex = DetermineHoveredView(LocalMousePosition, ViewRects);
	InputState.HoveredViewIndex = InvalidViewIndex;
	if (InputState.CapturedViewIndex != InvalidViewIndex && !IsViewActive(InputState.CapturedViewIndex))
		InputState = EndCapture(InputState);
	// Capture가 없으면 마지막 선택을 유지하고, 숨겨진 View만 레이아웃에 맞게 보정한다.
	if (InputState.CapturedViewIndex != InvalidViewIndex)
		ActiveViewIndex = InputState.CapturedViewIndex;
	if (!IsViewActive(ActiveViewIndex))
		ActiveViewIndex = LayoutMode == ELayoutMode::Single && IsViewActive(SingleViewIndex)
			? SingleViewIndex : InvalidViewIndex;
}

// Quad 레이아웃에서만 Core Splitter Drag 계산 결과를 상태에 반영한다.
void FViewportsPanel::ApplySplitterDrag(const EDragAxis Axis, const float DeltaPixels, const FVector2 WindowSize)
{
	// 동명 멤버가 전역 계산 함수를 가리므로 이 호출에만 전역 한정이 필요하다.
	SplitRatio = ::ApplySplitterDrag(SplitRatio, Axis, DeltaPixels, WindowSize, MinimumSplitRatio);
	ComputeViewRects(SplitRatio, WindowSize, ViewRects);
	SyncViewportRects();
}

// 우클릭 Capture 상태와 활성 View를 갱신하고, 활성 View의 Client에 카메라 입력을 넘긴다.
void FViewportsPanel::UpdateInput(
	const float DeltaTime,
	const FVector2 LocalMousePosition,
	float& InOutMoveSpeed,
	const float MouseSensitivity)
{
	// 휠은 읽으면 비워지므로, 이 패널 위에 있거나 우클릭 Capture 중일 때만 가져간다. 아니면 에셋 탭이 받는다
	const bool bTakeWheel = IsHovered() || InputState.CapturedViewIndex != InvalidViewIndex;
	const int WheelDelta = bTakeWheel ? FInputSystem::GetWheelDelta() : 0;
	if (ImGui::IsKeyReleased(ImGuiKey_MouseRight))
		InputState = EndCapture(InputState);
	const int32 PointerViewIndex = DetermineHoveredView(LocalMousePosition, ViewRects);
	// 우클릭 시작 위치를 Capture하고, Capture 밖에서는 좌클릭으로만 선택을 바꾼다.
	if (InputState.CapturedViewIndex == InvalidViewIndex && IsViewActive(PointerViewIndex))
	{
		if (FInputSystem::IsMousePressed(EMouseButton::Right))
			InputState = BeginCapture(InputState, PointerViewIndex);
		else if (FInputSystem::IsMousePressed(EMouseButton::Left))
			ActiveViewIndex = PointerViewIndex;
	}
	if (InputState.CapturedViewIndex != InvalidViewIndex)
		ActiveViewIndex = InputState.CapturedViewIndex;
	if (!IsViewActive(ActiveViewIndex)) return;

	// 활성 View의 Viewport가 지금 그리는 Client에 넘긴다. PIE View의 Game Client는 에디터 카메라 입력을 받지 않는다.
	const FRect& Rect = ViewRects[ActiveViewIndex];
	FViewportInput Input{};
	Input.MousePosition = {LocalMousePosition.X - Rect.X, LocalMousePosition.Y - Rect.Y};
	Input.DeltaTime = DeltaTime;
	Input.MouseSensitivity = MouseSensitivity;
	Input.MoveSpeed = InOutMoveSpeed;
	Input.WheelDelta = WheelDelta;
	Input.bCaptured = InputState.CapturedViewIndex == ActiveViewIndex;
	Input.bPointerOver = PointerViewIndex == ActiveViewIndex;
	Input.bEnabled = true;
	GetViewport(ActiveViewIndex).UpdateInput(Input);
	InOutMoveSpeed = Input.MoveSpeed;
}

// 유효 Rect와 Single 대상 인덱스 또는 Quad 모드로 View 활성 여부를 판정한다.
bool FViewportsPanel::IsViewActive(const int32 ViewIndex) const
{
	if (ViewIndex < 0 || ViewIndex >= ViewCount || !IsViewRectValid(ViewRects[ViewIndex]))
		return false;
	return LayoutMode == ELayoutMode::QuadSplit || ViewIndex == SingleViewIndex;
}

const FRect& FViewportsPanel::GetViewRect(const int32 ViewIndex) const
{
	assert(ViewIndex >= 0 && ViewIndex < ViewCount);
	return ViewRects[ViewIndex];
}

// 활성 View Rect 기준 로컬 좌표를 역투영한다.
bool FViewportsPanel::TryGetActiveViewRay(const FVector2 LocalMousePosition, FRay& OutRay) const
{
	if (ActiveViewIndex == InvalidViewIndex || !IsViewActive(ActiveViewIndex))
		return false;
	// Splitter gutter나 다른 View 위의 좌표를 이전 Active View의 Ray로 잘못 해석하지 않는다.
	if (DetermineHoveredView(LocalMousePosition, ViewRects) != ActiveViewIndex)
		return false;
	const FRect& Rect = ViewRects[ActiveViewIndex];
	const FVector2 ViewLocal{LocalMousePosition.X - Rect.X, LocalMousePosition.Y - Rect.Y};
	OutRay = GetViewport(ActiveViewIndex).Deproject(ViewLocal);
	return true;
}

// 렌더 타깃은 View가 처음 보일 때 Viewport가 만든다.
bool FViewportsPanel::Init()
{
	return true;
}

// 패널의 프레임 갱신 인터페이스이며 별도 계산은 하지 않는다.
void FViewportsPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
}

// 마우스 위치에서 패널 원점을 빼 로컬 좌표로 바꾼다.
FVector2 FViewportsPanel::GetLocalMousePosition() const
{
	const ImVec2 Mouse = ImGui::GetMousePos();
	return {Mouse.x - ContentOrigin.x, Mouse.y - ContentOrigin.y};
}

// 엔진 입력 처리는 ImGui NewFrame보다 먼저 실행되므로, 지난 프레임에 기록한 View 위 컨트롤 영역을 현재 OS 커서로 검사한다.
// 컨트롤 위의 클릭이 뷰포트 피킹(빈 곳 클릭 = 선택 해제)이나 기즈모 조작으로 새지 않게 한다.
bool FViewportsPanel::IsHovered() const
{
	POINT Cursor{};
	if (::GetCursorPos(&Cursor))
	{
		const float CursorX = static_cast<float>(Cursor.x);
		const float CursorY = static_cast<float>(Cursor.y);
		for (const FControlRect& Rect : ControlRects)
		{
			if (CursorX >= Rect.Min.x && CursorX < Rect.Max.x && CursorY >= Rect.Min.y && CursorY < Rect.Max.y)
				return false;
		}
	}
	return bHovered;
}

void FViewportsPanel::RecordLastItemAsControl()
{
	ControlRects.Add({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()});
}

// 누적 가로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeHorizontalDrag()
{
	const float Result = PendingHorizontalDrag;
	PendingHorizontalDrag = 0.0f;
	return Result;
}

// 누적 세로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeVerticalDrag()
{
	const float Result = PendingVerticalDrag;
	PendingVerticalDrag = 0.0f;
	return Result;
}

// 대기 Layout 요청을 한 번 반환하고 플래그를 지운다.
bool FViewportsPanel::ConsumeLayoutRequest(ELayoutMode& OutMode, int32& OutSingleViewIndex)
{
	if (!bHasLayoutRequest)
		return false;
	OutMode = RequestedLayoutMode;
	OutSingleViewIndex = RequestedSingleViewIndex;
	bHasLayoutRequest = false;
	return true;
}

// 대기 Preset 요청을 한 번 반환하고 지운다.
bool FViewportsPanel::ConsumeCameraPresetRequest(int32& OutViewIndex, EMultipleViewportsCameraPreset& OutPreset)
{
	if (PendingCameraPresetViewIndex == InvalidViewIndex)
		return false;
	OutViewIndex = PendingCameraPresetViewIndex;
	OutPreset = PendingCameraPreset;
	PendingCameraPresetViewIndex = InvalidViewIndex;
	return true;
}

// 현재 프리셋 이름을 단 드롭다운을 View 오른쪽 위에 두고, 펼치면 프리셋과 카메라 속성을 보여 준다.
void FViewportsPanel::DrawCameraMenu(const int32 ViewIndex, const ImVec2& ViewTopRight)
{
	const EMultipleViewportsCameraPreset CurrentPreset = GetEditorClient(ViewIndex).GetCameraPreset();
	// 어떤 프리셋 이름이 와도 잘리지 않게 가장 긴 이름에 맞춘다.
	const float CameraMenuWidth = CalcComboWidth(CameraPresetLabels, IM_ARRAYSIZE(CameraPresetLabels));
	ImGui::SetCursorScreenPos({ViewTopRight.x - CameraMenuWidth - ViewControlMargin, ViewTopRight.y + ViewControlMargin});
	ImGui::SetNextItemWidth(CameraMenuWidth);
	// 콤보 팝업은 기본으로 버튼 폭을 따르므로, 속성 칸이 들어가도록 최소 폭을 넓힌다.
	ImGui::SetNextWindowSizeConstraints({CameraMenuPopupWidth, 0.0f}, {FLT_MAX, FLT_MAX});
	const bool bOpen = ImGui::BeginCombo("##CameraMenu", CameraPresetLabels[static_cast<int>(CurrentPreset)]);
	RecordLastItemAsControl();
	if (!bOpen)
		return;

	// 프리셋은 고르면 메뉴가 닫히고, 다음 프레임에 엔진이 요청을 편집 Client에 반영한다.
	const auto PresetItem = [&](const EMultipleViewportsCameraPreset Preset)
	{
		if (ImGui::Selectable(CameraPresetLabels[static_cast<int>(Preset)], Preset == CurrentPreset))
		{
			PendingCameraPresetViewIndex = ViewIndex;
			PendingCameraPreset = Preset;
		}
	};
	ImGui::SeparatorText("Perspective");
	PresetItem(EMultipleViewportsCameraPreset::Perspective);
	ImGui::SeparatorText("Orthographic");
	for (int Preset = static_cast<int>(EMultipleViewportsCameraPreset::OrthographicView);
		Preset <= static_cast<int>(EMultipleViewportsCameraPreset::Right); ++Preset)
		PresetItem(static_cast<EMultipleViewportsCameraPreset>(Preset));

	ImGui::SeparatorText("View");
	DrawCameraProperties(ViewIndex);
	ImGui::EndCombo();
}

void FViewportsPanel::DrawCameraProperties(const int32 ViewIndex)
{
	FViewCamera Camera = GetEditorClient(ViewIndex).GetCamera();
	const bool bOrthographic = Camera.Projection.Mode == EProjectionMode::Orthographic;
	bool bChanged = false;
	bool bRotationChanged = false;

	if (!ImGui::BeginTable("CameraProperties", 2))
		return;
	ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, CameraPropertyLabelWidth);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	const auto BeginRow = [](const char* Label)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
	};

	// 직교는 FOV 대신 화면에 담기는 월드 폭을 바꾼다.
	if (bOrthographic)
	{
		BeginRow("Ortho Width");
		float OrthoWidth = Camera.Projection.OrthoWidth;
		if (ImGui::DragFloat("##OrthoWidth", &OrthoWidth, 0.1f) && std::isfinite(OrthoWidth))
		{
			Camera.Projection.OrthoWidth = FMath::Clamp(OrthoWidth, 0.01f, 1000000.0f);
			bChanged = true;
		}
	}
	else
	{
		BeginRow("Field of View");
		float Fov = Camera.Projection.FovDegrees;
		if (ImGui::DragFloat("##FOV", &Fov, 0.1f, 1.0f, 179.0f, "%.1f") && std::isfinite(Fov))
		{
			Camera.Projection.FovDegrees = FMath::Clamp(Fov, 1.0f, 179.0f);
			bChanged = true;
		}
	}

	// Near와 Far는 둘 다 맞아야(0 < Near < Far) 반영한다.
	BeginRow("Near View Plane");
	float Near = Camera.Projection.NearClip;
	bool bClipChanged = ImGui::InputFloat("##Near", &Near);
	BeginRow("Far View Plane");
	float Far = Camera.Projection.FarClip;
	bClipChanged |= ImGui::InputFloat("##Far", &Far);
	if (bClipChanged && std::isfinite(Near) && std::isfinite(Far) && Near > 0.0f && Far > Near)
	{
		Camera.Projection.NearClip = Near;
		Camera.Projection.FarClip = Far;
		bChanged = true;
	}

	BeginRow("Location");
	float Location[3] = {Camera.Transform.Location.X, Camera.Transform.Location.Y, Camera.Transform.Location.Z};
	if (ImGui::DragFloat3("##Location", Location, 0.1f) &&
		std::isfinite(Location[0]) && std::isfinite(Location[1]) && std::isfinite(Location[2]))
	{
		Camera.Transform.Location = {Location[0], Location[1], Location[2]};
		bChanged = true;
	}

	// 원근 카메라는 Roll 없이 Yaw·Pitch로만 조작하므로 Pitch를 제한하고 Roll을 0으로 둔다.
	BeginRow("Rotation");
	const FQuat CurrentRotation(Camera.Transform.Rotation.X, Camera.Transform.Rotation.Y, Camera.Transform.Rotation.Z, Camera.Transform.Rotation.W);
	FRotator Rotation = CurrentRotation.ToFRotator();
	float Euler[3] = {Rotation.Pitch, Rotation.Yaw, Rotation.Roll};
	if (ImGui::DragFloat3("##Rotation", Euler, 0.1f) &&
		std::isfinite(Euler[0]) && std::isfinite(Euler[1]) && std::isfinite(Euler[2]))
	{
		Rotation.Pitch = bOrthographic ? Euler[0] : FMath::Clamp(Euler[0], -89.0f, 89.0f);
		Rotation.Yaw = Euler[1];
		Rotation.Roll = bOrthographic ? Euler[2] : 0.0f;
		const FQuat NewRotation = Rotation.Quaternion();
		Camera.Transform.Rotation = {NewRotation.X, NewRotation.Y, NewRotation.Z, NewRotation.W};
		bChanged = true;
		bRotationChanged = true;
	}
	ImGui::SetItemTooltip("Pitch / Yaw / Roll");
	ImGui::EndTable();

	if (bChanged)
		GetEditorClient(ViewIndex).ApplyCameraProperties(Camera, bRotationChanged, ViewRects[ViewIndex]);
}

// 표시할 View와 값을 기록하고 표시 시작 시각을 갱신한다.
void FViewportsPanel::ShowCameraSpeed(const int32 ViewIndex, const float Speed)
{
	CameraSpeedViewIndex = ViewIndex;
	CameraSpeedValue = Speed;
	CameraSpeedShownTime = ImGui::GetTime();
}

// View 왼쪽 아래에 반투명 배경과 함께 이동 속도를 그린다.
void FViewportsPanel::DrawCameraSpeedNotice(ImDrawList* DrawList) const
{
	if (CameraSpeedViewIndex < 0 || CameraSpeedViewIndex >= 4 || !Slots[CameraSpeedViewIndex].bActive)
		return;
	if (ImGui::GetTime() - CameraSpeedShownTime > CameraSpeedNoticeSeconds)
		return;

	const FRect& Rect = ViewRects[CameraSpeedViewIndex];
	const FString Text = std::format("Camera Speed  {:.2f}", CameraSpeedValue);
	const ImVec2 TextSize = ImGui::CalcTextSize(Text.c_str());
	const ImVec2 TextMin{
		ContentOrigin.x + Rect.X + StatOverlayMargin + StatOverlayPadding,
		ContentOrigin.y + Rect.Y + Rect.Height - StatOverlayMargin - StatOverlayPadding - TextSize.y};
	DrawList->AddRectFilled(
		{TextMin.x - StatOverlayPadding, TextMin.y - StatOverlayPadding},
		{TextMin.x + TextSize.x + StatOverlayPadding, TextMin.y + TextSize.y + StatOverlayPadding},
		StatOverlayBackgroundColor, 4.0f);
	DrawList->AddText(TextMin, ValueColor, Text.c_str());
}

// View Texture와 Splitter·Layout·Preset UI를 그리고 요청을 기록한다.
void FViewportsPanel::OnRender()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
	const bool bVisible = ImGui::Begin("Viewports", nullptr,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoTitleBar);
	// 탭이 가려지면 크기를 0으로 둔다. 다음 프레임 레이아웃에서 모든 View가 비활성이 되어
	// 엔진이 렌더와 입력을 건너뛴다. 남아 있던 Splitter 드래그와 Hover도 버린다.
	if (!bVisible)
	{
		ContentSize = {};
		bHovered = false;
		ControlRects.Reset();
		bDraggingVerticalSplitter = false;
		bDraggingHorizontalSplitter = false;
		PendingHorizontalDrag = PendingVerticalDrag = 0.0f;
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	ContentOrigin = ImGui::GetCursorScreenPos();
	ContentSize = ImGui::GetContentRegionAvail();
	ContentSize.x = std::max(1.0f, ContentSize.x);
	ContentSize.y = std::max(1.0f, ContentSize.y);
	bHovered = ImGui::IsWindowHovered();
	ControlRects.Reset();

	// 전체 캔버스를 한 번 확보한 뒤 각 렌더 타깃을 창 DrawList에 직접 그린다.
	// Image 항목 네 개를 따로 배치하면 ImGui 레이아웃과 클리핑 상태가 삽입 순서에
	// 영향을 받아, Core가 올바른 사각형을 줘도 아래쪽 행이 잘릴 수 있다.
	ImGui::Dummy(ContentSize);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(ContentOrigin,
		{ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y}, true);
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
	{
		const FViewSlot& Slot = Slots[ViewIndex];
		FTexture2D* ColorTarget = Slot.Viewport.GetColorTarget();
		if (!Slot.bActive || !ColorTarget)
		{
			continue;
		}

		// 패널 레이아웃 좌표와 최종 표시용 렌더 타깃의 결합
		const FRect& Rect = ViewRects[ViewIndex];
		const ImVec2 ViewMin{ ContentOrigin.x + Rect.X, ContentOrigin.y + Rect.Y };
		const ImVec2 ViewMax{ ViewMin.x + Rect.Width, ViewMin.y + Rect.Height };
		DrawList->AddImage(ColorTarget->GetSRV(), ViewMin, ViewMax);
	}
	DrawList->PopClipRect();

	if (Slots[1].bActive || Slots[2].bActive || Slots[3].bActive)
	{
		// View Rect 사이에 비워 둔 gutter의 중앙에 Splitter 버튼을 배치한다.
		const float SplitX = (ViewRects[0].X + ViewRects[0].Width + ViewRects[1].X) * 0.5f;
		const float SplitY = (ViewRects[0].Y + ViewRects[0].Height + ViewRects[2].Y) * 0.5f;

		const ImVec2 VerticalMin{ContentOrigin.x + SplitX - SplitterThickness * 0.5f, ContentOrigin.y};
		const ImVec2 VerticalMax{VerticalMin.x + SplitterThickness, ContentOrigin.y + ContentSize.y};
		const ImVec2 HorizontalMin{ContentOrigin.x, ContentOrigin.y + SplitY - SplitterThickness * 0.5f};
		const ImVec2 HorizontalMax{ContentOrigin.x + ContentSize.x, HorizontalMin.y + SplitterThickness};

		ImGui::SetCursorScreenPos(VerticalMin);
		ImGui::InvisibleButton("##MultipleViewportsHorizontalSplitter", {SplitterThickness, ContentSize.y});
		ImGui::SetCursorScreenPos(HorizontalMin);
		ImGui::InvisibleButton("##MultipleViewportsVerticalSplitter", {ContentSize.x, SplitterThickness});

		const bool bVerticalHovered = bHovered && ImGui::IsMouseHoveringRect(VerticalMin, VerticalMax);
		const bool bHorizontalHovered = bHovered && ImGui::IsMouseHoveringRect(HorizontalMin, HorizontalMax);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			bDraggingVerticalSplitter = bVerticalHovered;
			bDraggingHorizontalSplitter = bHorizontalHovered;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			bDraggingVerticalSplitter = false;
			bDraggingHorizontalSplitter = false;
		}

		const bool bVerticalHighlighted = bDraggingVerticalSplitter || bVerticalHovered;
		const bool bHorizontalHighlighted = bDraggingHorizontalSplitter || bHorizontalHovered;
		DrawList->AddRectFilled(VerticalMin, VerticalMax,
			bVerticalHighlighted ? SplitterHoverColor : SplitterColor);
		DrawList->AddRectFilled(HorizontalMin, HorizontalMax,
			bHorizontalHighlighted ? SplitterHoverColor : SplitterColor);

		if (bDraggingVerticalSplitter)
			PendingHorizontalDrag += ImGui::GetIO().MouseDelta.x;
		if (bDraggingHorizontalSplitter)
			PendingVerticalDrag += ImGui::GetIO().MouseDelta.y;

		if (bVerticalHighlighted && bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		else if (bVerticalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		else if (bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	else
	{
		bDraggingVerticalSplitter = false;
		bDraggingHorizontalSplitter = false;
	}

	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		if (!Slots[ViewIndex].bActive)
			continue;
		ImGui::SetCursorScreenPos({
			ContentOrigin.x + ViewRects[ViewIndex].X + 8.0f,
			ContentOrigin.y + ViewRects[ViewIndex].Y + 8.0f});
		ImGui::PushID(100 + ViewIndex);
		// 왼쪽 컨트롤 줄 전체를 한 그룹으로 묶어 피킹을 막을 영역으로 기록한다.
		ImGui::BeginGroup();
		// 기즈모 좌표계는 모든 View가 공유한다. 크기 모드는 항상 Local 축을 쓴다.
		if (Gizmo)
		{
			const bool bWorldSpace = Gizmo->GetSpace() == EGizmoSpace::World;
			// 두 글자 중 긴 쪽에 맞춰 눌러도 버튼 폭이 바뀌지 않게 한다.
			const float GizmoSpaceButtonWidth = std::max(ImGui::CalcTextSize("World").x, ImGui::CalcTextSize("Local").x)
				+ ImGui::GetStyle().FramePadding.x * 2.0f;
			if (ImGui::Button(bWorldSpace ? "World" : "Local", {GizmoSpaceButtonWidth, 0.0f}))
				Gizmo->SetSpace(bWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World);
			ImGui::SetItemTooltip("Gizmo coordinate space (Scale always uses Local)");
			ImGui::SameLine();
		}
		// 조명·셰이딩 모델은 Lit일 때만 보인다.
        {
            FViewRenderMode RenderMode = Slots[ViewIndex].Viewport.GetViewRenderMode();
            bool bChanged = false;

            int ViewMode = static_cast<int>(RenderMode.ViewMode);
            ImGui::SetNextItemWidth(CalcComboWidth(ViewModeNames, IM_ARRAYSIZE(ViewModeNames)));
            if (ImGui::Combo("##ViewMode", &ViewMode, ViewModeNames, IM_ARRAYSIZE(ViewModeNames)))
            {
                RenderMode.ViewMode = static_cast<EViewMode>(ViewMode);
                bChanged = true;
            }
            ImGui::SameLine();

            if (RenderMode.ViewMode == EViewMode::Lit)
            {
                const char* LightingModelLabels[] = {"Lambert", "Phong", "Blinn-Phong"};
                int LightingModel = static_cast<int>(RenderMode.LightingModel);
                ImGui::SetNextItemWidth(CalcComboWidth(LightingModelLabels, IM_ARRAYSIZE(LightingModelLabels)));
                if (ImGui::Combo("##LightingModel", &LightingModel, LightingModelLabels, IM_ARRAYSIZE(LightingModelLabels)))
                {
                    RenderMode.LightingModel = static_cast<ELightingModel>(LightingModel);
                    bChanged = true;
                }
                ImGui::SetItemTooltip("Lighting Model");
                ImGui::SameLine();

                int ShadingModel = static_cast<int>(RenderMode.ShadingModel);
                ImGui::SetNextItemWidth(CalcComboWidth(ShadingModelNames, IM_ARRAYSIZE(ShadingModelNames)));
                if (ImGui::Combo("##ShadingModel", &ShadingModel, ShadingModelNames, IM_ARRAYSIZE(ShadingModelNames)))
                {
                    RenderMode.ShadingModel = static_cast<EShadingModel>(ShadingModel);
                    bChanged = true;
                }
                ImGui::SetItemTooltip("Shading Model");
                ImGui::SameLine();
            }

            if (bChanged)
                Slots[ViewIndex].Viewport.SetViewRenderMode(RenderMode);
        }
        if (LayoutMode == ELayoutMode::QuadSplit)
		{
			if (ImGui::SmallButton("Single"))
			{
				RequestedLayoutMode = ELayoutMode::Single;
				RequestedSingleViewIndex = ViewIndex;
				bHasLayoutRequest = true;
			}
		}
		else if (ViewIndex == SingleViewIndex && ImGui::SmallButton("Quad"))
		{
			RequestedLayoutMode = ELayoutMode::QuadSplit;
			RequestedSingleViewIndex = ViewIndex;
			bHasLayoutRequest = true;
		}
		if (ViewIndex == GetEditorViewIndex() &&
			FStatOverlay::IsAnyEnabled() &&
			(FStatOverlay::IsEnabled(EStatFlags::Profile) ||
				FStatRegistry::Find(EditorStats::STAT_PickingTime)))
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset Stats"))
				FStatRegistry::Reset();
		}
		ImGui::EndGroup();
		RecordLastItemAsControl();

		DrawCameraMenu(ViewIndex, {
			ContentOrigin.x + ViewRects[ViewIndex].X + ViewRects[ViewIndex].Width,
			ContentOrigin.y + ViewRects[ViewIndex].Y});
		ImGui::PopID();
	}

	// 마지막으로 선택된 뷰포트만 오버레이
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		if (ViewIndex != GetEditorViewIndex()) continue;
		if (!Slots[ViewIndex].bActive)
			continue;
		DrawStatOverlay(DrawList, {
			ContentOrigin.x + ViewRects[ViewIndex].X,
			ContentOrigin.y + ViewRects[ViewIndex].Y});
	}
	DrawCameraSpeedNotice(DrawList);

	ImGui::End();
	ImGui::PopStyleVar();
}

// 항목별 줄을 모아 한 번에 배경과 텍스트를 그린다.
void FViewportsPanel::DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const
{
	if (!DrawList || !FStatOverlay::IsAnyEnabled())
		return;

	constexpr float BytesPerMegabyte = 1024.0f * 1024.0f;

	// 제목은 UE처럼 노란색, 값은 흰색으로 구분한다.
	struct FStatLine
	{
		FString Text;
		ImU32 Color;
	};
	TArray<FStatLine> Lines;

	if (FStatOverlay::IsEnabled(EStatFlags::FPS))
	{
		Lines.Add({"FPS", TitleColor});
		Lines.Add({std::format("  {:.1f} fps", FStatOverlay::GetFPS()), ValueColor});
		Lines.Add({std::format("  {:.2f} ms", FStatOverlay::GetFrameTimeMs()), ValueColor});
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Memory))
	{
		Lines.Add({"Memory", TitleColor});
		Lines.Add({std::format("  Object  {:.2f} MB ({} allocs)",
			static_cast<double>(FStatOverlay::GetObjectAllocationBytes()) / BytesPerMegabyte,
			FStatOverlay::GetObjectAllocationCount()), ValueColor});
		Lines.Add({std::format("  Process {:.2f} MB",
			static_cast<double>(FStatOverlay::GetProcessWorkingSetBytes()) / BytesPerMegabyte), ValueColor});
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Shadow))
	{
		Lines.Add({ "Shadow", TitleColor });
		const FShadowStats& shadow = FStatOverlay::GetShadowStats();

		Lines.Add({ std::format("  Lights  Dir {} / Point {} / Spot {}",
			shadow.DirectionalLightCount, shadow.PointLightCount, shadow.SpotLightCount), ValueColor });
		Lines.Add({ std::format("  Maps Rendered  {}", shadow.NumShadowMapsRendered), ValueColor });
		Lines.Add({ std::format("  Draw Calls     {}", shadow.NumShadowDrawCalls), ValueColor });
		Lines.Add({ std::format("  Skipped Lights {}", shadow.NumSkippedLights), ValueColor });
		Lines.Add({ std::format("  Skipped Casters {}", shadow.NumSkippedCasterDraws), ValueColor });
		Lines.Add({ std::format("  Cached Maps {}", shadow.NumCachedShadowMaps), ValueColor });
		Lines.Add({ std::format("  Memory         {:.2f} MB",
			static_cast<int64>(shadow.ShadowMapMemoryBytes) / BytesPerMegabyte), ValueColor });
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Decal))
	{
		const FDecalStats& Decal = FStatOverlay::GetDecalStats();

		Lines.Add({ "Decal", TitleColor });
		Lines.Add({ std::format("  Draw Calls (All Views): {}", Decal.DrawCount), ValueColor});
		Lines.Add({ std::format("  CPU Submission: {:.2f} ms", Decal.DrawTime), ValueColor});
	}

	struct FProfileStatLine
	{
		FString Text;
		ImU32 Color;
		FString Values[3];
	};
	TArray<FProfileStatLine> ProfileLines;
	if (FStatOverlay::IsEnabled(EStatFlags::Profile))
	{
		ProfileLines.Add({"CPU Profile (ms)", TitleColor, {"Last", "Avg", "Max"}});

		for (const auto& [Name, Data] : FStatRegistry::GetAll())
		{
			if (TStatId{ Name } == EditorStats::STAT_PickingTime)
				continue;

			FProfileStatLine Line{"  " + FString(Name), ValueColor};
			Line.Values[0] = std::format("{:.2f}", Data.GetLastMs());
			Line.Values[1] = std::format("{:.2f}", Data.GetRecentAverageMs());
			Line.Values[2] = std::format("{:.2f}", Data.GetMaxMs());
			ProfileLines.Add(Line);
		}
	}

	if (const FCycleStatData* PickingData = FStatRegistry::Find(EditorStats::STAT_PickingTime))
	{
		ProfileLines.Add({ "Picking", TitleColor });
		FProfileStatLine Line{"  Time (ms)", ValueColor};
		Line.Values[0] = std::format("Last: {:.2f}", PickingData->GetLastMs());
		Line.Values[1] = std::format("Attempts: {}", PickingData->CallCount);
		Line.Values[2] = std::format("Acc.: {:.2f}", PickingData->GetTotalMs());
		ProfileLines.Add(Line);
	}

	if (Lines.Num() == 0 && ProfileLines.Num() == 0)
		return;

	// 제어 위젯 한 줄 아래에서 시작해 Combo와 겹치지 않게 한다.
	const float LineHeight = ImGui::GetTextLineHeight();
	const ImVec2 Origin{
		ViewMin.x + StatOverlayMargin,
		ViewMin.y + StatOverlayMargin + ImGui::GetFrameHeight() + StatOverlayMargin};

	// 최대 너비 계산
	float MaxWidth = 0.0f;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
		MaxWidth = std::max(MaxWidth, ImGui::CalcTextSize(Lines[Index].Text.c_str()).x);

	const float MinValueWidth = ImGui::CalcTextSize("00000.00").x;
	float NameWidth = 0.0f;
	float ValueWidths[3] = { MinValueWidth, MinValueWidth, MinValueWidth };
	bool bHasColumns = false;

	for (const FProfileStatLine& Line : ProfileLines)
	{
		const float TextWidth = ImGui::CalcTextSize(Line.Text.c_str()).x;
		bHasColumns = true;
		NameWidth = std::max(NameWidth, TextWidth);
		for (int32 Column = 0; Column < 3; ++Column)
			ValueWidths[Column] = std::max(MinValueWidth, ImGui::CalcTextSize(Line.Values[Column].c_str()).x);
	}

	float ColumnRight[3]{};
	if (bHasColumns)
	{
		float Width = NameWidth;
		for (int32 Column = 0; Column < 3; ++Column)
		{
			Width += ImGui::GetFontSize() + ValueWidths[Column];
			ColumnRight[Column] = Origin.x + Width;
		}
		MaxWidth = std::max(MaxWidth, Width);
	}

	const ImVec2 BackgroundMin{Origin.x - StatOverlayPadding, Origin.y - StatOverlayPadding};
	const ImVec2 BackgroundMax{
		Origin.x + MaxWidth + StatOverlayPadding,
		Origin.y + LineHeight * static_cast<float>(Lines.Num() + ProfileLines.Num()) + StatOverlayPadding};
	DrawList->AddRectFilled(BackgroundMin, BackgroundMax, StatOverlayBackgroundColor, 4.0f);

	// Lines 그리기
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		DrawList->AddText(
			{ Origin.x, Origin.y + LineHeight * static_cast<float>(Index) },
			Lines[Index].Color,
			Lines[Index].Text.c_str());
	}

	// ProfileLines 그리기
	for (int32 Index = 0; Index < ProfileLines.Num(); ++Index)
	{
		const FProfileStatLine& Line = ProfileLines[Index];
		const float Y = Origin.y + LineHeight * static_cast<float>(Lines.Num() + Index);
		DrawList->AddText({ Origin.x, Y }, Line.Color, Line.Text.c_str());

		for (int32 Column = 0; Column < 3; ++Column)
		{
			const FString& Text = Line.Values[Column];
			const float TextWidth = ImGui::CalcTextSize(Text.c_str()).x;
			DrawList->AddText({ ColumnRight[Column] - TextWidth, Y }, Line.Color, Text.c_str());
		}
	}
}
