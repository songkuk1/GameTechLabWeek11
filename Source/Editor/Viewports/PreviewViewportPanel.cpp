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

	DrawToolbar();
	bHovered = false;
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	RequestedSize = { std::max(0.0f, std::floor(Avail.x)), std::max(0.0f, std::floor(Avail.y)) };

	ImageSize = {};
	if (Viewport->IsVisible() && Viewport->GetColorTarget())
	{
		ImGui::Image((ImTextureID)Viewport->GetColorTarget()->GetSRV(), { RequestedSize.X, RequestedSize.Y });
		ImageMin = ImGui::GetItemRectMin();
		ImageSize = ImGui::GetItemRectSize();
		bHovered = ImGui::IsItemHovered();
	}
	ImGui::End();
}
