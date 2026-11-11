#include "EnginePCH.h"
#include "BlendSpaceEditorTab.h"

#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Asset/AssetManager.h"
#include "Render/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"

#include "GameFramework/Actor/SkeletalMeshActor.h"

#include "Editor/Viewports/PreviewViewportPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Viewports/ViewportCameraMath.h"

#include "UObject/UObjectIterator.h"
#include "Camera/CameraTypes.h"
#include "Serialization/JsonArchive.h"
#include "Core/PathUtils.h"
#include "Engine/World.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr float FramingPitchDegrees = 20.0f;

    FEditorTabSettings MakeBlendSpaceEditorSettings(const UBlendSpace1D* BlendSpace)
    {
        const FString AssetPath = BlendSpace ? BlendSpace->GetPath() : "Unknown";

        const size_t NameStart = AssetPath.find_last_of("/\\");

        FString AssetName = NameStart == FString::npos ? AssetPath : AssetPath.substr(NameStart + 1);

        const size_t ExtensionStart = AssetName.rfind('.');

        if (ExtensionStart != FString::npos)
        {
            AssetName.resize(ExtensionStart);
        }

        FEditorTabSettings Settings;
        Settings.Title = "BlendSpace: " + AssetName;
        Settings.DockSpaceName = "BlendSpaceEditorDock_" + AssetPath;
        Settings.bClosable = true;
        Settings.bIsolatePanels = true;

        return Settings;
    }
}

FBlendSpaceEditorTab::FBlendSpaceEditorTab(UBlendSpace1D* InBlendSpace) : FEditorTab(MakeBlendSpaceEditorSettings(InBlendSpace)), BlendSpace(InBlendSpace)
{
    if (!BlendSpace)
    {
        return;
    }

    EditableAxis = BlendSpace->GetAxis();

    const FString InitialAxisName = EditableAxis.Name.ToString();

    std::snprintf(AxisNameBuffer, sizeof(AxisNameBuffer), "%s", InitialAxisName.c_str());

    PreviewScene = MakeUnique<FPreviewScene>();

    SkeletalMesh = BlendSpace->GetPreviewMesh();

    if (!SkeletalMesh)
    {
        for (TObjectIterator<USkeletalMesh> It; It; ++It)
        {
            USkeletalMesh* Candidate = *It;

            if (Candidate && Candidate->GetSkeleton() == BlendSpace->GetSkeleton())
            {
                SkeletalMesh = Candidate;
                break;
            }
        }

        if (SkeletalMesh && BlendSpace->SetPreviewMesh(SkeletalMesh))
        {
            bDirty = true;
        }
    }

    if (SkeletalMesh)
    {
        if (UWorld* PreviewWorld = GetWorld())
        {
            PreviewActor = Cast<ASkeletalMeshActor>(PreviewWorld->SpawnActor(ASkeletalMeshActor::StaticClass()));
        }

        if (PreviewActor)
        {
            USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

            if (Component)
            {
                Component->SetSkeletalMesh(SkeletalMesh);
            }
        }
    }

    if (BlendSpace && BlendSpace->GetSkeleton())
    {
        USkeletalMesh* PreviewMesh = BlendSpace->GetSkeleton()->GetPreviewMesh();
        FString MeshPathStr = PreviewMesh ? PreviewMesh->GetPath() : BlendSpace->GetSkeleton()->GetPath();
        std::filesystem::path BaseDir = PathUtils::FromUtf8(MeshPathStr).parent_path();
        if (BaseDir.empty() || BaseDir.string().find("Assets") == std::string::npos)
        {
            BaseDir = std::filesystem::path("Assets") / BaseDir;
        }

        if (std::filesystem::exists(BaseDir))
        {
            for (const auto& Entry : std::filesystem::recursive_directory_iterator(BaseDir))
            {
                if (Entry.is_regular_file())
                {
                    const auto Ext = Entry.path().extension();
                    if (_wcsicmp(Ext.c_str(), L".fbx") == 0)
                    {
                        UAssetManager::Get().LoadFbxModel(PathUtils::ToUtf8(Entry.path()), false, true, false);
                    }
                    else if (_wcsicmp(Ext.c_str(), L".anim") == 0)
                    {
                        UAssetManager::Get().LoadAnimSequence(PathUtils::ToUtf8(Entry.path()));
                    }
                }
            }
        }
    }

    for (TObjectIterator<UAnimSequence> It; It; ++It)
    {
        UAnimSequence* Animation = *It;

        if (Animation)
        {
            USkeleton* TargetSkel = BlendSpace->GetSkeleton();
            if (Animation->GetSkeleton() == TargetSkel || (TargetSkel && TargetSkel->IsCompatibleWith(Animation->GetSkeleton())))
            {
                if (Animation->GetSkeleton() != TargetSkel && TargetSkel)
                {
                    Animation->RetargetToSkeleton(TargetSkel);
                }
                AvailableAnimations.Add(Animation);
            }
        }
    }

    std::sort(AvailableAnimations.begin(),  AvailableAnimations.end(), [](const UAnimSequence* A, const UAnimSequence* B) { return A->GetPath() < B->GetPath(); });

    if (!AvailableAnimations.IsEmpty())
    {
        NewSampleAnimation = AvailableAnimations[0];
    }

    const FBlendSpaceAxis1D& Axis = BlendSpace->GetAxis();

    NewSampleValue = (Axis.MinValue + Axis.MaxValue) * 0.5f;
    BlendInput = Axis.MinValue;

    if (PreviewActor && !BlendSpace->GetSamples().IsEmpty())
    {
        USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

        if (Component)
        {
            Component->PlayBlendSpace(BlendSpace, BlendInput);
            bPlaying = true;
        }
        else
        {
            bPlaying = false;
        }
    }
    else
    {
        bPlaying = false;
    }

    ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("BlendSpaceViewport##") + BlendSpace->GetPath(), [this]() { DrawToolbar(); });
}

FBlendSpaceEditorTab::~FBlendSpaceEditorTab()
{
    Panels.Empty();

    ViewportPanel = nullptr;
    PreviewActor = nullptr;
}

UWorld* FBlendSpaceEditorTab::GetWorld() const
{
    return PreviewScene ? PreviewScene->GetWorld() : nullptr;
}

void FBlendSpaceEditorTab::Tick(float DeltaTime)
{
    FEditorTab::Tick(DeltaTime);

    if (PreviewActor && bPlaying)
    {
        PreviewActor->GetSkeletalMeshComponent()->TickAnimation(DeltaTime);
    }

    if (!bFramed && ViewportPanel && ViewportPanel->GetViewport()->IsVisible() && SkeletalMesh)
    {
        FrameMesh();
        bFramed = true;
    }

    if (GetWorld())
    {
        GetWorld()->GetScene().UpdateAllTransforms();
    }
}

void FBlendSpaceEditorTab::RouteInput(const FViewportInput& BaseInput)
{
    if (ViewportPanel)
    {
        ViewportPanel->RouteInput(BaseInput);
    }
}

void FBlendSpaceEditorTab::CollectViews(TArray<FEditorTabView>& OutViews)
{
    if (!ViewportPanel)
    {
        return;
    }

    FEditorTabView View;
    View.Viewport = ViewportPanel->GetViewport();
    View.OverlayClient = ViewportPanel->GetEditorClient();

    OutViews.Add(View);
}

void FBlendSpaceEditorTab::FrameMesh()
{
    if (!PreviewActor || !SkeletalMesh)
    {
        return;
    }

    USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

    const FBox Bounds = SkeletalMesh->GetMeshData().AABB.GetWorldAABB(Component->GetWorldMatrix());

    const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
    const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;

    const float Radius = std::sqrt(HalfExtent.X * HalfExtent.X + HalfExtent.Y * HalfExtent.Y + HalfExtent.Z * HalfExtent.Z);

    const FMinimalViewInfo DefaultView;

    FViewCamera Camera{};
    Camera.Projection = 
    {
        .Mode = EProjectionMode::Perspective,
        .FovDegrees = DefaultView.FOV,
        .OrthoWidth = DefaultView.OrthoWidth,
        .NearClip = DefaultView.NearClip,
        .FarClip = std::max(DefaultView.FarClip, Radius * 20.0f)
    };

    Camera.Transform.Rotation = ViewportCameraMath::MakeCameraRotation(0.0f, -FramingPitchDegrees);

    FEditorViewportClient* Client = ViewportPanel->GetEditorClient();

    Client->InitializeCamera(Camera, EMultipleViewportsCameraPreset::Perspective);

    Client->FocusOn(Center, Radius, ViewportPanel->GetViewport()->GetOutputRect());
}

void FBlendSpaceEditorTab::RefreshPreview(float PreviousTime)
{
    if (!PreviewActor)
    {
        return;
    }

    USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

    if (BlendSpace->GetSamples().IsEmpty())
    {
        Component->PlayAnimation(nullptr);
        bPlaying = false;
        return;
    }

    Component->PlayBlendSpace(BlendSpace, BlendInput);
    Component->SetAnimationPlaybackTime(PreviousTime);
}

bool FBlendSpaceEditorTab::CanCloseNow()
{
    if (!bDirty)
    {
        return true;
    }

    bShowClosePopup = true;
    return false;
}

bool FBlendSpaceEditorTab::SaveBlendSpaceAsset()
{
    if (!BlendSpace || BlendSpace->GetPath().empty())
    {
        ErrorMessage = "Invalid BlendSpace asset path.";
        return false;
    }

    const FString FilePath = PathUtils::ToUtf8(fs::path("Assets") / PathUtils::FromUtf8(BlendSpace->GetPath()));

    if (!FJsonArchive::SaveBlendSpace(BlendSpace, FilePath))
    {
        ErrorMessage = "Could not save the BlendSpace file.";
        return false;
    }

    bDirty = false;
    ErrorMessage.clear();

    return true;
}

void FBlendSpaceEditorTab::DrawToolbar()
{
    if (!BlendSpace)
    {
        ImGui::TextUnformatted("No BlendSpace asset.");
        return;
    }

    if (bShowClosePopup)
    {
        ImGui::OpenPopup("Unsaved BlendSpace Changes");
        bShowClosePopup = false;
    }

    if (ImGui::BeginPopupModal("Unsaved BlendSpace Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("There are unsaved changes.");
        ImGui::TextUnformatted("Do you want to save before closing?");
        if (!ErrorMessage.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "%s", ErrorMessage.c_str());
        }
        if (ImGui::Button("Save"))
        {
            if (SaveBlendSpaceAsset())
            {
                SetOpen(false);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            bDirty = false;
            SetOpen(false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::TextUnformatted("BlendSpace 1D");

    const FBlendSpaceAxis1D& Axis = BlendSpace->GetAxis();

    // Axis 설정
    if (ImGui::CollapsingHeader("Axis Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputText("Axis Name", AxisNameBuffer, sizeof(AxisNameBuffer));
        ImGui::InputFloat("Minimum Value", &EditableAxis.MinValue, 1.0f, 10.0f,  "%.1f");
        ImGui::InputFloat("Maximum Value", &EditableAxis.MaxValue, 1.0f, 10.0f, "%.1f");
        ImGui::InputInt("Grid Divisions", &EditableAxis.GridDivisions);

        if (ImGui::Button("Apply Axis"))
        {
            FBlendSpaceAxis1D NewAxis = EditableAxis;
            NewAxis.Name = FName(AxisNameBuffer);
            if (NewAxis.Name.IsNone() || !std::isfinite(NewAxis.MinValue) || !std::isfinite(NewAxis.MaxValue) || NewAxis.MinValue >= NewAxis.MaxValue ||  NewAxis.GridDivisions < 1 || NewAxis.GridDivisions > 64)
            {
                ErrorMessage = "Invalid Axis settings. Grid Divisions must be 1~64.";
            }
            else
            {
                bool bSamplesOutsideRange = false;

                for (const FBlendSample1D& Sample : BlendSpace->GetSamples())
                {
                    if (Sample.SampleValue < NewAxis.MinValue || Sample.SampleValue > NewAxis.MaxValue)
                    {
                        bSamplesOutsideRange = true;
                        break;
                    }
                }

                if (bSamplesOutsideRange)
                {
                    ErrorMessage = "Move or remove samples outside the new Axis range first.";
                }
                else
                {
                    const bool bAxisChanged = NewAxis.Name != Axis.Name || NewAxis.MinValue != Axis.MinValue || NewAxis.MaxValue != Axis.MaxValue || NewAxis.GridDivisions != Axis.GridDivisions;

                    if (BlendSpace->SetAxis(NewAxis))
                    {
                        EditableAxis = NewAxis;

                        if (bAxisChanged)
                        {
                            bDirty = true;
                        }

                        BlendInput = std::clamp(BlendInput, NewAxis.MinValue, NewAxis.MaxValue);

                        NewSampleValue = std::clamp(NewSampleValue, NewAxis.MinValue, NewAxis.MaxValue);

                        if (PreviewActor)
                        {
                            USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

                            if (Component)
                            {
                                Component->SetAnimationBlendInput(BlendInput);
                            }
                        }

                        ErrorMessage.clear();
                    }
                    else
                    {
                        ErrorMessage = "Failed to apply Axis settings.";
                    }
                }
            }
        }
    }

    const FString AxisName = Axis.Name.ToString();

    ImGui::Text("Current Axis: %s (%.1f ~ %.1f)", AxisName.c_str(), Axis.MinValue, Axis.MaxValue);

    if (!SkeletalMesh || !PreviewActor)
    {
        ImGui::TextDisabled("No compatible Skeletal Mesh found for this Skeleton.");
    }

    ImGui::Separator();

    const FString SelectedAnimationName = NewSampleAnimation ? NewSampleAnimation->GetPath() : "Select Animation";

    if (ImGui::BeginCombo("New Animation",  SelectedAnimationName.c_str()))
    {
        for (UAnimSequence* Animation : AvailableAnimations)
        {
            const bool bSelected = Animation == NewSampleAnimation;

            if (ImGui::Selectable(Animation->GetPath().c_str(), bSelected))
            {
                NewSampleAnimation = Animation;
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::DragFloat("New Sample Position", &NewSampleValue, 1.0f, Axis.MinValue, Axis.MaxValue, "%.1f");

    ImGui::DragFloat("New Sample Rate", &NewSampleRateScale, 0.01f,  0.01f, 10.0f, "%.2fx");

    const float PreviousTime = PreviewActor ? PreviewActor->GetSkeletalMeshComponent()->GetAnimationPlaybackTime() : 0.0f;

    bool bSamplesChanged = false;

    if (ImGui::Button("Add Sample"))
    {
        ErrorMessage.clear();

        const bool bWasEmpty = BlendSpace->GetSamples().IsEmpty();

        if (!NewSampleAnimation)
        {
            ErrorMessage = "Select an Animation before adding a sample.";
        }
        else if (BlendSpace->AddSample(NewSampleValue, NewSampleAnimation, NewSampleRateScale))
        {
            if (bWasEmpty)
            {
                bPlaying = true;
            }

            bSamplesChanged = true;
            bDirty = true;

            const TArray<FBlendSample1D>& UpdatedSamples = BlendSpace->GetSamples();

            float PreviousValue = Axis.MinValue;
            float BestGap = -1.0f;
            float NextSampleValue = (Axis.MinValue + Axis.MaxValue) * 0.5f;

            for (const FBlendSample1D& Sample : UpdatedSamples)
            {
                const float Value = std::clamp(Sample.SampleValue, Axis.MinValue, Axis.MaxValue);

                const float Gap = Value - PreviousValue;

                if (Gap > BestGap)
                {
                    BestGap = Gap;
                    NextSampleValue = (PreviousValue + Value) * 0.5f;
                }

                PreviousValue = std::max(PreviousValue, Value);
            }

            const float LastGap = Axis.MaxValue - PreviousValue;

            if (LastGap > BestGap)
            {
                BestGap = LastGap;
                NextSampleValue = (PreviousValue + Axis.MaxValue) * 0.5f;
            }

            if (BestGap > 2.0e-4f)
            {
                NewSampleValue = NextSampleValue;
            }
        }
        else
        {
            ErrorMessage = "Could not add sample. Check the position, Skeleton and Rate Scale.";
        }
    }

    // BlendSpace 1D Graph
    ImGui::Separator();
    const FString GraphAxisName = Axis.Name.ToString();

    ImGui::Text("Blend Graph  |  %s  |  Input: %.1f",  GraphAxisName.c_str(), BlendInput);
    const float GraphWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    constexpr float GraphHeight = 96.0f;

    ImGui::InvisibleButton("##BlendSpaceGraph", ImVec2(GraphWidth, GraphHeight));

    const ImVec2 GraphMin = ImGui::GetItemRectMin();
    const ImVec2 GraphMax = ImGui::GetItemRectMax();

    const bool bGraphHovered = ImGui::IsItemHovered();
    const bool bGraphActive = ImGui::IsItemActive();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    const float TrackLeft = GraphMin.x + 12.0f;
    const float TrackRight = GraphMax.x - 12.0f;
    const float TrackWidth = std::max(1.0f, TrackRight - TrackLeft);
    const float TrackY = (GraphMin.y + GraphMax.y) * 0.5f;

    const float AxisRange = Axis.MaxValue - Axis.MinValue;

    auto ValueToScreenX = [&](float Value)
        {
            const float Alpha = std::clamp((Value - Axis.MinValue) / AxisRange, 0.0f, 1.0f);
            return TrackLeft + TrackWidth * Alpha;
        };

    auto ScreenXToValue = [&](float X)
        {
            const float Alpha = std::clamp((X - TrackLeft) / TrackWidth, 0.0f, 1.0f);
            return Axis.MinValue + AxisRange * Alpha;
        };

    DrawList->AddRectFilled(GraphMin, GraphMax, IM_COL32(35, 38, 44, 255), 4.0f);

    DrawList->AddLine(ImVec2(TrackLeft, TrackY), ImVec2(TrackRight, TrackY), IM_COL32(150, 150, 150, 255), 2.0f);

    const int32 LabelStep = std::max(1, Axis.GridDivisions / 6);

    for (int32 Index = 0; Index <= Axis.GridDivisions; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / Axis.GridDivisions;
        const float X = TrackLeft + TrackWidth * Alpha;
        DrawList->AddLine(ImVec2(X, TrackY - 8.0f), ImVec2(X, TrackY + 8.0f), IM_COL32(110, 115, 125, 255), 1.0f);

        if (Index % LabelStep == 0 || Index == Axis.GridDivisions)
        {
            const float TickValue = Axis.MinValue + AxisRange * Alpha;
            char TickLabel[32];
            std::snprintf(TickLabel, sizeof(TickLabel), "%.1f", TickValue);
            const ImVec2 TextSize = ImGui::CalcTextSize(TickLabel);
            DrawList->AddText(ImVec2(X - TextSize.x * 0.5f, TrackY + 12.0f), IM_COL32(190, 195, 205, 255), TickLabel);
        }
    }

    const TArray<FBlendSample1D>& GraphSamples = BlendSpace->GetSamples();

    if (bGraphHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 MousePos = ImGui::GetIO().MousePos;
        const float InputX = ValueToScreenX(BlendInput);

        if (std::abs(MousePos.x - InputX) <= 8.0f)
        {
            bDraggingBlendInput = true;
            DraggingSampleIndex = -1;
        }
        else
        {
            int32 NearestIndex = -1;
            float NearestDistance = 10.0f;

            for (int32 Index = 0; Index < GraphSamples.Num(); ++Index)
            {
                const float X = ValueToScreenX(GraphSamples[Index].SampleValue);
                const float Distance = std::abs(MousePos.x - X);

                if (Distance < NearestDistance)
                {
                    NearestDistance = Distance;
                    NearestIndex = Index;
                }
            }

            if (NearestIndex != -1)
            {
                DraggingSampleIndex = NearestIndex;
                DraggingSampleValue = GraphSamples[NearestIndex].SampleValue;
            }
        }
    }

    if (bDraggingBlendInput)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && bGraphActive)
        {
            BlendInput = ScreenXToValue(ImGui::GetIO().MousePos.x);

            if (PreviewActor)
            {
                USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

                if (Component)
                {
                    Component->SetAnimationBlendInput(BlendInput);
                }
            }
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            bDraggingBlendInput = false;
        }
    }

    if (DraggingSampleIndex != -1)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemActive())
        {
            DraggingSampleValue = ScreenXToValue(ImGui::GetIO().MousePos.x);
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (GraphSamples.IsValidIndex(DraggingSampleIndex))
            {
                const float OriginalValue = GraphSamples[DraggingSampleIndex].SampleValue;

                if (std::abs(OriginalValue - DraggingSampleValue) > 1.0e-4f)
                {
                    if (BlendSpace->SetSampleValue(DraggingSampleIndex, DraggingSampleValue))
                    {
                        bSamplesChanged = true;
                        bDirty = true;
                        ErrorMessage.clear();
                    }
                    else
                    {
                        ErrorMessage = "Another sample already uses this position.";
                    }
                }
            }

            DraggingSampleIndex = -1;
        }
    }

    for (int32 Index = 0; Index < GraphSamples.Num(); ++Index)
    {
        const FBlendSample1D& Sample = GraphSamples[Index];

        const float Value = Index == DraggingSampleIndex ? DraggingSampleValue : Sample.SampleValue;

        const float X = ValueToScreenX(Value);

        const ImVec2 MousePosition = ImGui::GetIO().MousePos;

        const bool bSelected = Index == DraggingSampleIndex;

        const bool bMarkerHovered = bGraphHovered && std::abs(MousePosition.x - X) <= 9.0f && std::abs(MousePosition.y - TrackY) <= 12.0f;

        const ImU32 MarkerColor = bSelected ? IM_COL32(80, 200, 255, 255) : bMarkerHovered ? IM_COL32(255, 225, 130, 255) : IM_COL32(255, 190, 70, 255);

        DrawList->AddCircleFilled(ImVec2(X, TrackY), bSelected || bMarkerHovered ? 7.0f : 6.0f, MarkerColor);

        char SampleLabel[16];

        std::snprintf(SampleLabel, sizeof(SampleLabel), "S%d", Index + 1);

        const ImVec2 LabelSize = ImGui::CalcTextSize(SampleLabel);

        float LabelX = X - LabelSize.x * 0.5f;

        if (LabelX < GraphMin.x + 2.0f)
        {
            LabelX = GraphMin.x + 2.0f;
        }

        if (LabelX + LabelSize.x > GraphMax.x - 2.0f)
        {
            LabelX = GraphMax.x - LabelSize.x - 2.0f;
        }

        DrawList->AddText(ImVec2(LabelX, TrackY - 27.0f), MarkerColor, SampleLabel);

        if (bMarkerHovered)
        {
            const FString AnimationPath = Sample.Animation ? Sample.Animation->GetPath() : "None";
            ImGui::SetTooltip("%s\nPosition: %.1f\nRate Scale: %.2fx", AnimationPath.c_str(), Value, Sample.RateScale);
        }
    }

    const float InputX = ValueToScreenX(BlendInput);

    const ImU32 InputColor = IM_COL32(255, 75, 75, 255);

    // 현재 입력 위치
    DrawList->AddLine(ImVec2(InputX, GraphMin.y + 5.0f), ImVec2(InputX, GraphMax.y - 5.0f), InputColor, 2.5f);

    DrawList->AddCircleFilled(ImVec2(InputX, GraphMin.y + 7.0f), 5.0f, InputColor);

    ImGui::Separator();

    ImGui::Text("Samples: %d", BlendSpace->GetSamples().Num());
    ImGui::Separator();

    int32 PendingRemoveIndex = -1;
    int32 PendingPositionIndex = -1;
    float PendingPosition = 0.0f;

    const TArray<FBlendSample1D>& Samples = BlendSpace->GetSamples();

    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        const FBlendSample1D& Sample = Samples[Index];

        ImGui::PushID(Index);

        ImGui::Text("Sample %d", Index + 1);

        const FString AnimationName = Sample.Animation ? Sample.Animation->GetPath() : "None";

        if (ImGui::BeginCombo("Animation", AnimationName.c_str()))
        {
            for (UAnimSequence* Animation : AvailableAnimations)
            {
                const bool bSelected = Sample.Animation == Animation;

                if (ImGui::Selectable(Animation->GetPath().c_str(), bSelected))
                {
                    if (BlendSpace->ReplaceSampleAnimation(Index, Animation))
                    {
                        bSamplesChanged = true;
                        bDirty = true;
                    }
                }
            }

            ImGui::EndCombo();
        }

        float SampleValue = Sample.SampleValue;

        if (ImGui::DragFloat("Position", &SampleValue, 1.0f, Axis.MinValue, Axis.MaxValue, "%.1f"))
        {
            PendingPositionIndex = Index;
            PendingPosition = SampleValue;
        }

        float RateScale = Sample.RateScale;

        if (ImGui::DragFloat("Rate Scale", &RateScale, 0.01f, 0.01f, 10.0f, "%.2fx"))
        {
            if (BlendSpace->SetSampleRateScale(Index, RateScale))
            {
                bSamplesChanged = true;
                bDirty = true;
            }
        }

        if (ImGui::Button("Remove"))
        {
            PendingRemoveIndex = Index;
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    if (PendingRemoveIndex != -1)
    {
        if (BlendSpace->RemoveSample(PendingRemoveIndex))
        {
            bSamplesChanged = true;
            bDirty = true;
        }
    }
    else if (PendingPositionIndex != -1)
    {
        if (BlendSpace->SetSampleValue(PendingPositionIndex, PendingPosition))
        {
            bSamplesChanged = true;
            bDirty = true;
        }
        else
        {
            ErrorMessage = "Another sample already uses this position.";
        }
    }

    if (bSamplesChanged)
    {
        RefreshPreview(PreviousTime);
    }

    if (!ErrorMessage.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "%s", ErrorMessage.c_str());
    }

    ImGui::Separator();

    ImGui::BeginDisabled(BlendSpace->GetSamples().IsEmpty() || !PreviewActor);

    if (ImGui::SliderFloat("Blend Input", &BlendInput, Axis.MinValue, Axis.MaxValue, "%.1f"))
    {
        PreviewActor->GetSkeletalMeshComponent()->SetAnimationBlendInput(BlendInput);
    }

    if (ImGui::Button(bPlaying ? "Pause" : "Play"))
    {
        bPlaying = !bPlaying;
    }

    ImGui::SameLine();

    if (ImGui::Button("Restart"))
    {
        PreviewActor->GetSkeletalMeshComponent()->SetAnimationPlaybackTime(0.0f);
        bPlaying = true;
    }

    ImGui::EndDisabled();
    ImGui::Separator();

    ImGui::BeginDisabled(!bDirty);

    if (ImGui::Button("Save BlendSpace"))
    {
        SaveBlendSpaceAsset();
    }

    ImGui::EndDisabled();

    if (bDirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Unsaved changes");
    }
}