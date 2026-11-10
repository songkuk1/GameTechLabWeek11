#include "EnginePCH.h"
#include "Editor/Viewports/PreviewViewportPanel.h"

#include "Editor/Viewports/EditorViewportClient.h"
#include "Input/InputSystem.h"

#include <algorithm>
#include <cmath>

FPreviewViewportPanel::FPreviewViewportPanel(UWorld* InWorld, FString InPanelName, std::function<void()> InDrawToolbar)
	: World(InWorld), PanelName(std::move(InPanelName)), DrawToolbar(std::move(InDrawToolbar))
{
}

bool FPreviewViewportPanel::Init()
{
	Viewport = MakeUnique<FViewport>();
	TUniquePtr<FEditorViewportClient> Client = MakeUnique<FEditorViewportClient>();
	Client->SetWorld(World);
	EditorClient = Client.get();
	Viewport->SetEditorClient(std::move(Client));
	return true;
}

void FPreviewViewportPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
	const FVector2 Current = Viewport->GetOutputSize();
	if (RequestedSize.X != Current.X || RequestedSize.Y != Current.Y)
		Viewport->SetOutputSize(static_cast<uint32>(RequestedSize.X), static_cast<uint32>(RequestedSize.Y));
}

void FPreviewViewportPanel::RouteInput(FViewportInput BaseInput)
{
	if (!FInputSystem::IsMouseDown(EMouseButton::Right))
		bCaptured = false;
	else if (bHovered && FInputSystem::IsMousePressed(EMouseButton::Right))
		bCaptured = true;

	BaseInput.bCaptured = bCaptured;
	BaseInput.bPointerOver = bHovered;
	BaseInput.bEnabled = BaseInput.bEnabled && (bHovered || bCaptured);
	// 휠은 한 곳만 가져가야 한다. 이 창 위에 있을 때만 소비한다
	BaseInput.WheelDelta = BaseInput.bEnabled ? FInputSystem::GetWheelDelta() : 0;
	Viewport->UpdateInput(BaseInput);
}

bool FPreviewViewportPanel::GetMouseViewportPosition(FVector2& OutPosition) const
{
	const FVector2 OutputSize = Viewport->GetOutputSize();
	if (ImageSize.x <= 0.0f || ImageSize.y <= 0.0f || !Viewport->IsVisible())
		return false;

	// 이미지와 출력 크기는 한 프레임 어긋날 수 있어 비율로 맞춘다
	const ImVec2 Mouse = ImGui::GetIO().MousePos;
	OutPosition = { (Mouse.x - ImageMin.x) * OutputSize.X / ImageSize.x, (Mouse.y - ImageMin.y) * OutputSize.Y / ImageSize.y };
	return true;
}

void FPreviewViewportPanel::OnRender()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
	const bool bVisible = ImGui::Begin(PanelName.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();
	if (!bVisible)
	{
		RequestedSize = {};
		ImageSize = {};
		bHovered = false;
		ImGui::End();
		return;
	}

	const ImVec2 Available = ImGui::GetContentRegionAvail();

	constexpr float SplitterHeight = 8.0f;
	constexpr float MinViewportHeight = 80.0f;
	constexpr float MinToolbarHeight = 48.0f;

	const float ItemSpacingY = ImGui::GetStyle().ItemSpacing.y;

	const float MaxAllowedToolbarHeight = std::max(0.0f, Available.y - MinViewportHeight - SplitterHeight - ItemSpacingY * 2.0f);

	if (!bToolbarHeightInitialized && Available.y > MinViewportHeight + SplitterHeight + ItemSpacingY * 2.0f)
	{
		ToolbarHeight = std::min(MaxAllowedToolbarHeight, Available.y * 0.4f);
		bToolbarHeightInitialized = true;
	}

	const float EffectiveMinToolbarHeight = std::min(MinToolbarHeight, MaxAllowedToolbarHeight);
	ToolbarHeight = std::clamp(ToolbarHeight, EffectiveMinToolbarHeight, MaxAllowedToolbarHeight);

	const float ViewportHeight = std::max(0.0f, Available.y - ToolbarHeight - SplitterHeight - ItemSpacingY * 2.0f);

	RequestedSize = {std::max(0.0f, std::floor(Available.x)), std::max(0.0f, std::floor(ViewportHeight))};

	bHovered = false;
	ImageSize = {};

	if (Viewport->IsVisible() && Viewport->GetColorTarget())
	{
		ImGui::Image((ImTextureID)Viewport->GetColorTarget()->GetSRV(), { RequestedSize.X, RequestedSize.Y });
		ImageMin = ImGui::GetItemRectMin();
		ImageSize = ImGui::GetItemRectSize();
		bHovered = ImGui::IsItemHovered();
	}
	else
	{
		ImGui::Dummy({ RequestedSize.X, RequestedSize.Y });
	}

	ImGui::InvisibleButton("##PreviewToolbarSplitter", ImVec2(Available.x, SplitterHeight));

	const bool bSplitterHovered = ImGui::IsItemHovered();
	const bool bSplitterActive = ImGui::IsItemActive();

	const ImVec2 SplitterMin = ImGui::GetItemRectMin();
	const ImVec2 SplitterMax = ImGui::GetItemRectMax();
	const float SplitterCenterY = (SplitterMin.y + SplitterMax.y) * 0.5f;

	const ImU32 SplitterColor = bSplitterActive ? IM_COL32(100, 170, 230, 255)
		: bSplitterHovered ? IM_COL32(115, 125, 145, 255)
		: IM_COL32(75, 80, 90, 255);

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImVec2((SplitterMin.x + SplitterMax.x) * 0.5f - 20.0f, SplitterCenterY - 2.0f),
		ImVec2((SplitterMin.x + SplitterMax.x) * 0.5f + 20.0f, SplitterCenterY + 2.0f),
		SplitterColor,
		2.0f);

	if (bSplitterHovered || bSplitterActive)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	if (bSplitterActive)
	{
		ToolbarHeight = std::clamp(ToolbarHeight - ImGui::GetIO().MouseDelta.y, EffectiveMinToolbarHeight, MaxAllowedToolbarHeight);
	}

	ImGui::BeginChild("PreviewToolbar", ImVec2(0.0f, ToolbarHeight), true);

	if (DrawToolbar)
	{
		DrawToolbar();
	}
	ImGui::EndChild();
	ImGui::End();
}
