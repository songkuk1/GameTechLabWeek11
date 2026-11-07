#include "EnginePCH.h"
#include "BlendSpaceEditorTab.h"

#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
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

    for (TObjectIterator<UAnimSequence> It; It; ++It)
    {
        UAnimSequence* Animation = *It;

        if (Animation && Animation->GetSkeleton() == BlendSpace->GetSkeleton())
        {
            AvailableAnimations.Add(Animation);
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

    ImGui::Text("Axis: %s (%.1f ~ %.1f)", Axis.Name.ToString().c_str(), Axis.MinValue, Axis.MaxValue);

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

    ImGui::Separator();

    ImGui::Text("Samples: %d", BlendSpace->GetSamples().Num());

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