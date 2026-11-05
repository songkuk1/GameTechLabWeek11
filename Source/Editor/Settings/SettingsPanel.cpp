#include "EnginePCH.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"

#include "Engine/World.h"

// 종료 시 렌더·에디터·뷰포트 설정을 함께 저장한다.
FSettingsPanel::~FSettingsPanel()
{
	// 종료 시 다른 객체를 참조하지 않고 보존한 설정만 저장한다.
	ViewportsPanel = nullptr;
	SaveSettings();
}

// 설정 파일을 읽어 패널 상태를 초기화한다.
bool FSettingsPanel::Init()
{
	LoadSettings();
	return true;
}


// 설정 패널의 프레임 갱신 진입점이다.
void FSettingsPanel::Tick(float DeltaTime)
{
}

// ImGui 조작으로 렌더·카메라 설정과 저장·복원 요청을 처리한다.
void FSettingsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Settings");

	// 슬라이더 폭을 창 전체에서 같게 맞춘다
	ImGui::PushItemWidth(200.0f);

	//////////////////////////////////////////////////////////

	// 화면에 무엇을 그릴지 (언리얼의 Show 플래그)
	ImGui::SeparatorText("Show");

	ImGui::Checkbox("Primitives", &Settings.bDrawPrimitives);
	ImGui::Checkbox("Skeletal Mesh", &Settings.bDrawSkeletalMeshes);
	ImGui::Checkbox("Decals", &Settings.bDrawDecals);
	ImGui::Checkbox("Bounding Boxes", &Settings.bDrawBoundingBox);
	ImGui::Checkbox("Object UUID", &Settings.bShowUUID);
	ImGui::Checkbox("Grid", &Settings.bDrawGrid);
	ImGui::Checkbox("Exponential Height Fog", &Settings.bExponentialHeightFog);
	ImGui::Checkbox("Tile Light Count", &Settings.bVisualizeTileLightCount);
	ImGui::Checkbox("Shadow", &Settings.bDrawShadows);
	ImGui::Checkbox("Visualize Cascades", &Settings.bVisualizeCascades);

	ImGui::Checkbox("Vignetting", &Settings.bVignetting);
	ImGui::BeginDisabled(!Settings.bVignetting);
	ImGui::Indent();
	ImGui::SliderFloat("Vignette Intensity", &Settings.VignetteIntensity, 0.0f, 1.0f, "%.1f");
	ImGui::SliderFloat("Vignette Radius", &Settings.VignetteRadius, 0.0f, 2.0f, "%.1f");
	ImGui::SliderFloat("Vignette Softness", &Settings.VignetteSoftness, 0.01f, 1.0f, "%.1f");
	ImGui::Unindent();
	ImGui::EndDisabled();

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Rendering");

	ImGui::TextDisabled("Fill mode is configured per viewport.");
	ImGui::Checkbox("FXAA", &Settings.bEnableFXAA);
	ImGui::Checkbox("Depth View", &Settings.bDepthView);
	ImGui::Checkbox("Linear Workflow", &Settings.bLinearWorkflow);
	ImGui::SetItemTooltip("Off: no sRGB decode/encode (gamma-space rendering), for comparison.");
	// 톤매핑은 View별 Lit에서만 적용되므로, 보이는 View 중 Lit이 하나라도 있어야 조작할 의미가 있다
	bool bAnyLitView = ViewportsPanel == nullptr;
	for (int32 Index = 0; ViewportsPanel && Index < FViewportsPanel::ViewCount; ++Index)
	{
		if (ViewportsPanel->IsViewActive(Index) && ViewportsPanel->GetViewport(Index).GetViewRenderMode().ViewMode == EViewMode::Lit)
		{
			bAnyLitView = true;
			break;
		}
	}

	ImGui::BeginDisabled(!Settings.bLinearWorkflow || !bAnyLitView);
	ImGui::Checkbox("Tone Mapping (ACES)", &Settings.bToneMapping);
	ImGui::SetItemTooltip("Compresses HDR scene color with ACES before sRGB encoding. Lit view only.");
	ImGui::BeginDisabled(!Settings.bToneMapping);
	ImGui::SliderFloat("Exposure (EV)", &Settings.ExposureEV, -4.0f, 4.0f, "%+.1f");
	ImGui::SetItemTooltip("Scales scene color by 2^EV before ACES tone mapping. Lit view only.");
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Shadow Filter");

	ImGui::BeginDisabled(!Settings.bDrawShadows);
	ImGui::Combo("Filter Mode", &Settings.ShadowFilterMode, ShadowFilterModeNames, static_cast<int>(EShadowFilterMode::Count));
	ImGui::SetItemTooltip("Hard: Point 1 tap / HardwarePCF: Comparison 1 tap / PCF: fixed radius / PCSS: variable radius");

	// 1탭 방식(Hard, HardwarePCF)에는 탭 배치·가중치·회전이 의미가 없다
	const bool bMultiTap = Settings.ShadowFilterMode >= static_cast<int32>(EShadowFilterMode::PCF);
	ImGui::BeginDisabled(!bMultiTap);
	ImGui::Checkbox("Poisson", &Settings.bShadowPoisson);
	ImGui::SetItemTooltip("Off: 5x5 grid taps");
	ImGui::Checkbox("Gaussian", &Settings.bShadowGaussian);
	ImGui::BeginDisabled(!Settings.bShadowPoisson);
	ImGui::Checkbox("Pixel Rotation", &Settings.bShadowRotate);
	ImGui::EndDisabled();
	if (Settings.ShadowFilterMode == static_cast<int32>(EShadowFilterMode::PCF))
	{
		ImGui::SliderFloat("PCF Radius (texels)", &Settings.ShadowPCFRadiusTexels, 0.5f, 8.0f, "%.1f");
	}
	else
	{
		ImGui::SliderFloat("Max Filter (texels)", &Settings.ShadowMaxFilterTexels, 1.0f, 64.0f, "%.0f");
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Grid");

	ImGui::BeginDisabled(!Settings.bDrawGrid);
	ImGui::SliderInt("Spacing", &Settings.GridSpacing, 1, 100);
	ImGui::EndDisabled();

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Camera");

	ImGui::SliderFloat("Move Speed", &Settings.CameraSpeed, MinimumCameraMoveSpeed, MaximumCameraMoveSpeed, "%.2f", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Rotate Sensitivity", &Settings.MouseSensitivity, 0.01f, 1.0f, "%.2f");

	//////////////////////////////////////////////////////////

	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::Separator();

	if (ImGui::Button("Save"))
	{
		SaveSettings();
	}
	ImGui::SetItemTooltip("Save settings to editor.ini");

	ImGui::SameLine();

	if (ImGui::Button("Load"))
	{
		LoadSettings();
	}
	ImGui::SetItemTooltip("Load settings from editor.ini");

	//////////////////////////////////////////////////////////

	ImGui::PopItemWidth();
	ImGui::End();
}

// 알려진 설정 섹션을 editor.ini에 함께 기록해 뷰포트 값 유실을 막는다.
bool FSettingsPanel::SaveSettings() const
{
	FEditorSettings Snapshot = Settings;
	ReadViewportSettings(Snapshot);
	std::ofstream File("editor.ini");

	if (!File.is_open())
	{
		HTR_LOG(Error, "Failed to open editor.ini for saving.");
		return false;
	}

	// 섹션은 Settings 패널의 섹션과 같은 순서로 쓴다
	File << "[Show]\n";
	File << "DrawPrimitives=" << Settings.bDrawPrimitives << "\n";
	File << "DrawSkeletalMeshes=" << Settings.bDrawSkeletalMeshes << "\n";
	File << "DrawDecals=" << Settings.bDrawDecals << '\n';
	File << "DrawBoundingBox=" << Settings.bDrawBoundingBox << "\n";
	File << "ShowUUID=" << Settings.bShowUUID << "\n";
	File << "DrawGrid=" << Settings.bDrawGrid << "\n";
	File << "ExponentialHeightFog=" << Settings.bExponentialHeightFog << "\n";
	File << "VisualizeCascades=" << Settings.bVisualizeCascades << "\n";
	File << "\n";

	File << "[Rendering]\n";
	File << "EnableFXAA=" << Settings.bEnableFXAA << "\n";
	File << "DepthView=" << Settings.bDepthView << "\n";
	File << "DrawShadows=" << Settings.bDrawShadows << "\n";
	File << "LinearWorkflow=" << Settings.bLinearWorkflow << "\n";
	File << "ToneMapping=" << Settings.bToneMapping << "\n";
	File << "ExposureEV=" << Settings.ExposureEV << "\n";
	File << "\n";

	File << "[ShadowFilter]\n";
	File << "ShadowFilterMode=" << ShadowFilterModeNames[FMath::Clamp(Settings.ShadowFilterMode, 0, static_cast<int32>(EShadowFilterMode::Count) - 1)] << "\n";
	File << "ShadowPoisson=" << Settings.bShadowPoisson << "\n";
	File << "ShadowGaussian=" << Settings.bShadowGaussian << "\n";
	File << "ShadowRotate=" << Settings.bShadowRotate << "\n";
	File << "ShadowPCFRadiusTexels=" << Settings.ShadowPCFRadiusTexels << "\n";
	File << "ShadowMaxFilterTexels=" << Settings.ShadowMaxFilterTexels << "\n";
	File << "\n";

	File << "[Grid]\n";
	File << "GridSpacing=" << Settings.GridSpacing << "\n";
	File << "\n";

	File << "[Camera]\n";
	File << "CameraMoveSpeed=" << Settings.CameraSpeed << "\n";
	File << "CameraRotateSensitivity=" << Settings.MouseSensitivity << "\n";
	File << "\n";

	File << "[MultipleViewports]\n";
	File << "Horizontal=" << Snapshot.MultipleViewportsHorizontal << "\n";
	File << "Vertical=" << Snapshot.MultipleViewportsVertical << "\n";
	File << "Layout=" << (Snapshot.bMultipleViewportsSingle ? "Single" : "Quad") << "\n";
	File << "SingleViewIndex=" << Snapshot.MultipleViewportsSingleViewIndex << "\n";

	for (int32 Index = 0; Index < 4; ++Index)
    {
        File << "View" << Index << "Fov=" << Snapshot.ViewFov[Index] << "\n";
        File << "View" << Index << "OrthoWidth=" << Snapshot.ViewOrthoWidth[Index] << "\n";
        File << "View" << Index << "Preset=" << Snapshot.ViewPreset[Index] << "\n";
        const FViewRenderMode& RenderMode = Snapshot.ViewRenderMode[Index];
        File << "View" << Index << "ViewMode=" << ViewModeNames[static_cast<int32>(RenderMode.ViewMode)] << "\n";
        File << "View" << Index << "LightingModel=" << LightingModelNames[static_cast<int32>(RenderMode.LightingModel)] << "\n";
        File << "View" << Index << "ShadingModel=" << ShadingModelNames[static_cast<int32>(RenderMode.ShadingModel)] << "\n";
        File << "View" << Index << "LocationX=" << Snapshot.ViewLocation[Index].X << "\n";
        File << "View" << Index << "LocationY=" << Snapshot.ViewLocation[Index].Y << "\n";
        File << "View" << Index << "LocationZ=" << Snapshot.ViewLocation[Index].Z << "\n";
        File << "View" << Index << "RotationX=" << Snapshot.ViewRotation[Index].X << "\n";
        File << "View" << Index << "RotationY=" << Snapshot.ViewRotation[Index].Y << "\n";
        File << "View" << Index << "RotationZ=" << Snapshot.ViewRotation[Index].Z << "\n";
        File << "View" << Index << "RotationW=" << Snapshot.ViewRotation[Index].W << "\n";
    }
    File.close();
	return true;
}

// 파일의 키를 파싱해 렌더·카메라·레이아웃 설정을 복원한다.
bool FSettingsPanel::LoadSettings()
{
	std::ifstream File("editor.ini");
	if (!File.is_open())
	{
		return false;
	}

	uint8 LocationComponentMasks[4]{};
	uint8 RotationComponentMasks[4]{};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Settings.bViewLocationSaved[Index] = false;
		Settings.bViewRotationSaved[Index] = false;
	}

	std::string Line;
	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == ';' || Line[0] == '[') continue;

		std::istringstream Iss(Line);
		std::string Key;
		if (std::getline(Iss, Key, '='))
		{
			std::string ValueStr;
			if (std::getline(Iss, ValueStr))
			{
				// 뷰 모드 값은 이름이라 아래 숫자 파싱 전에 읽는다. 모르는 이름은 무시한다.
				for (int32 Index = 0; Index < 4; ++Index)
				{
					FString Prefix = "View";
					Prefix += static_cast<char>('0' + Index);
					FViewRenderMode& RenderMode = Settings.ViewRenderMode[Index];
					if (Key == Prefix + "ViewMode")
						TryParseModeName(ValueStr, ViewModeNames, RenderMode.ViewMode);
					else if (Key == Prefix + "LightingModel")
						TryParseModeName(ValueStr, LightingModelNames, RenderMode.LightingModel);
					else if (Key == Prefix + "ShadingModel")
						TryParseModeName(ValueStr, ShadingModelNames, RenderMode.ShadingModel);
				}
				// 새 View 키는 NaN·잘못된 숫자·범위 밖 값을 무시한다.
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    FString Prefix = "View";
                    Prefix += static_cast<char>('0' + Index);
                    std::istringstream Number(ValueStr);
                    float Value = 0;
                    if (!(Number >> Value) || !std::isfinite(Value)) continue;
                    if (Key == Prefix + "Fov" && Value >= 1 && Value <= 179)
                        Settings.ViewFov[Index] = Value;
                    else if (Key == Prefix + "OrthoWidth" && Value >= 0.01f)
                        // 파일 값은 절대 상한으로 먼저 제한하고, 실제 View 비율 상한은 Adapter가 적용한다.
                        Settings.ViewOrthoWidth[Index] = FMath::Clamp(Value, 0.01f, 1000000.0f);
                    else if (Key == Prefix + "Preset" && Value >= 0 && Value <= 7 && Value == static_cast<int32>(Value))
                        Settings.ViewPreset[Index] = static_cast<int32>(Value);
                    else if (Key == Prefix + "LocationX")
                    {
                        Settings.ViewLocation[Index].X = Value;
                        LocationComponentMasks[Index] |= 1 << 0;
                    }
                    else if (Key == Prefix + "LocationY")
                    {
                        Settings.ViewLocation[Index].Y = Value;
                        LocationComponentMasks[Index] |= 1 << 1;
                    }
                    else if (Key == Prefix + "LocationZ")
                    {
                        Settings.ViewLocation[Index].Z = Value;
                        LocationComponentMasks[Index] |= 1 << 2;
                    }
                    else if (Key == Prefix + "RotationX")
                    {
                        Settings.ViewRotation[Index].X = Value;
                        RotationComponentMasks[Index] |= 1 << 0;
                    }
                    else if (Key == Prefix + "RotationY")
                    {
                        Settings.ViewRotation[Index].Y = Value;
                        RotationComponentMasks[Index] |= 1 << 1;
                    }
                    else if (Key == Prefix + "RotationZ")
                    {
                        Settings.ViewRotation[Index].Z = Value;
                        RotationComponentMasks[Index] |= 1 << 2;
                    }
                    else if (Key == Prefix + "RotationW")
                    {
                        Settings.ViewRotation[Index].W = Value;
                        RotationComponentMasks[Index] |= 1 << 3;
                    }
                }
				if (Key == "DrawPrimitives") Settings.bDrawPrimitives = std::stoi(ValueStr);
				else if (Key == "DrawSkeletalMeshes")
				{
					Settings.bDrawSkeletalMeshes = ValueStr != "0";
				}
				else if (Key == "DrawDecals") Settings.bDrawDecals = std::stoi(ValueStr);
				else if (Key == "DrawBoundingBox") Settings.bDrawBoundingBox = std::stoi(ValueStr);
				else if (Key == "ShowUUID") Settings.bShowUUID = std::stoi(ValueStr);
				else if (Key == "DrawGrid") Settings.bDrawGrid = std::stoi(ValueStr);
				else if (Key == "ExponentialHeightFog") Settings.bExponentialHeightFog = std::stoi(ValueStr);
				else if (Key == "VisualizeCascades") Settings.bVisualizeCascades = std::stoi(ValueStr);

				else if (Key == "EnableFXAA") Settings.bEnableFXAA = std::stoi(ValueStr);
				else if (Key == "DepthView") Settings.bDepthView = std::stoi(ValueStr);
				else if (Key == "DrawShadows") Settings.bDrawShadows = std::stoi(ValueStr);
				else if (Key == "LinearWorkflow") Settings.bLinearWorkflow = std::stoi(ValueStr);
				else if (Key == "ToneMapping") Settings.bToneMapping = std::stoi(ValueStr);
				else if (Key == "ExposureEV") Settings.ExposureEV = std::stof(ValueStr);

				else if (Key == "ShadowFilterMode")
				{
					// 이름으로 저장한다. 모르는 이름이면 기본값(PCSS)을 유지한다.
					EShadowFilterMode Mode = static_cast<EShadowFilterMode>(Settings.ShadowFilterMode);
					if (TryParseModeName(ValueStr, ShadowFilterModeNames, Mode))
						Settings.ShadowFilterMode = static_cast<int32>(Mode);
				}
				else if (Key == "ShadowPoisson") Settings.bShadowPoisson = std::stoi(ValueStr);
				else if (Key == "ShadowGaussian") Settings.bShadowGaussian = std::stoi(ValueStr);
				else if (Key == "ShadowRotate") Settings.bShadowRotate = std::stoi(ValueStr);
				else if (Key == "ShadowPCFRadiusTexels") Settings.ShadowPCFRadiusTexels = FMath::Clamp(std::stof(ValueStr), 0.5f, 8.0f);
				else if (Key == "ShadowMaxFilterTexels") Settings.ShadowMaxFilterTexels = FMath::Clamp(std::stof(ValueStr), 1.0f, 64.0f);

				else if (Key == "GridSpacing") Settings.GridSpacing = std::stoi(ValueStr);

				else if (Key == "CameraMoveSpeed") Settings.CameraSpeed = std::stof(ValueStr);
				else if (Key == "CameraRotateSensitivity") Settings.MouseSensitivity = std::stof(ValueStr);
				else if (Key == "Horizontal") Settings.MultipleViewportsHorizontal = std::stof(ValueStr);
				else if (Key == "Vertical") Settings.MultipleViewportsVertical = std::stof(ValueStr);
				else if (Key == "Layout") Settings.bMultipleViewportsSingle = ValueStr == "Single";
				else if (Key == "SingleViewIndex")
				{
					const int32 ViewIndex = std::stoi(ValueStr);
					Settings.MultipleViewportsSingleViewIndex = ViewIndex >= 0 && ViewIndex < 4 ? ViewIndex : 0;
				}
			}
		}
	}

	File.close();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Settings.bViewLocationSaved[Index] = LocationComponentMasks[Index] == 0x7;
		if (RotationComponentMasks[Index] == 0xF)
		{
			const FQuat& Rotation = Settings.ViewRotation[Index];
			const float LengthSquared = Rotation.X * Rotation.X + Rotation.Y * Rotation.Y +
				Rotation.Z * Rotation.Z + Rotation.W * Rotation.W;
			if (LengthSquared > 1.0e-12f)
			{
				Settings.ViewRotation[Index] = Rotation.Normalized();
				Settings.bViewRotationSaved[Index] = true;
			}
		}
	}
	ApplyViewportSettings();
	return true;
}

// 초기화된 카메라에 파일 설정을 적용하고 미저장 값은 초기 설정으로 채운다.
void FSettingsPanel::SetViewportsPanel(FViewportsPanel* Value)
{
    ViewportsPanel = Value;
    ApplyViewportSettings();
    CaptureViewportSettings();
}

// Transform·투영·표시·레이아웃을 저장용 설정에 복사한다.
void FSettingsPanel::ReadViewportSettings(FEditorSettings& Out) const
{
    if (!ViewportsPanel) return;
    Out.MultipleViewportsHorizontal = ViewportsPanel->GetSplitRatio().Horizontal;
    Out.MultipleViewportsVertical = ViewportsPanel->GetSplitRatio().Vertical;
    Out.bMultipleViewportsSingle = ViewportsPanel->GetLayoutMode() == ELayoutMode::Single;
    Out.MultipleViewportsSingleViewIndex = ViewportsPanel->GetSingleViewIndex();
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FEditorViewportClient& Client = ViewportsPanel->GetEditorClient(Index);
        const auto& Camera = Client.GetCamera();
        Out.ViewFov[Index] = Camera.Projection.FovDegrees;
        Out.ViewOrthoWidth[Index] = Camera.Projection.OrthoWidth;
        Out.ViewPreset[Index] = static_cast<int32>(Client.GetCameraPreset());
        Out.ViewRenderMode[Index] = ViewportsPanel->GetViewport(Index).GetViewRenderMode();
        Out.ViewLocation[Index] = Camera.Transform.Location;
        Out.ViewRotation[Index] = Camera.Transform.Rotation;
        Out.bViewLocationSaved[Index] = true;
        Out.bViewRotationSaved[Index] = true;
    }
}

// 종료 시 ViewportsPanel을 읽지 않도록 살아 있는 동안 저장용 설정을 갱신한다.
void FSettingsPanel::CaptureViewportSettings() { ReadViewportSettings(Settings); }

// 저장 프리셋·투영·표시·뷰 모드를 슬롯별로 복원하며 저장되지 않은 값은 초기값을 쓴다.
void FSettingsPanel::ApplyViewportSettings()
{
    if (!ViewportsPanel) return;
    ViewportsPanel->SetSplitRatio({Settings.MultipleViewportsHorizontal, Settings.MultipleViewportsVertical});
    ViewportsPanel->SetSingleViewIndex(Settings.MultipleViewportsSingleViewIndex);
    ViewportsPanel->SetLayoutMode(Settings.bMultipleViewportsSingle ? ELayoutMode::Single : ELayoutMode::QuadSplit);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FEditorViewportClient& Client = ViewportsPanel->GetEditorClient(Index);
        const FRect& Rect = ViewportsPanel->GetViewRect(Index);
        if (Settings.ViewPreset[Index] >= 0 && Settings.ViewPreset[Index] <= 7)
            Client.ApplyCameraPreset(static_cast<EMultipleViewportsCameraPreset>(Settings.ViewPreset[Index]), Rect);
        FViewCamera Camera = Client.GetCamera();
        if (Settings.ViewFov[Index] > 0) Camera.Projection.FovDegrees = Settings.ViewFov[Index];
        if (Settings.ViewOrthoWidth[Index] > 0) Camera.Projection.OrthoWidth = Settings.ViewOrthoWidth[Index];
        if (Settings.bViewLocationSaved[Index]) Camera.Transform.Location = Settings.ViewLocation[Index];
        if (Settings.bViewRotationSaved[Index]) Camera.Transform.Rotation = Settings.ViewRotation[Index];
        Client.ApplyCameraProperties(Camera, false, Rect);
        ViewportsPanel->GetViewport(Index).SetViewRenderMode(Settings.ViewRenderMode[Index]);
    }
}
