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

    for (TObjectIterator<USkeletalMesh> It; It; ++It)
    {
        USkeletalMesh* Candidate = *It;

        if (Candidate &&  Candidate->GetSkeleton() == BlendSpace->GetSkeleton())
        {
            SkeletalMesh = Candidate;
            break;
        }
    }

    if (SkeletalMesh)
    {
        PreviewActor = Cast<ASkeletalMeshActor>(GetWorld()->SpawnActor(ASkeletalMeshActor::StaticClass()));

        USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

        Component->SetSkeletalMesh(SkeletalMesh);

        for (TObjectIterator<UAnimSequence> It; It; ++It)
        {
            UAnimSequence* Animation = *It;

            if (Animation && Animation->GetSkeleton() == BlendSpace->GetSkeleton())
            {
                AvailableAnimations.Add(Animation);
            }
        }

        std::sort(AvailableAnimations.begin(), AvailableAnimations.end(), [](const UAnimSequence* A, const UAnimSequence* B) { return A->GetPath() < B->GetPath(); });

        if (!AvailableAnimations.IsEmpty())
        {
            NewSampleAnimation = AvailableAnimations[0];
        }

        const FBlendSpaceAxis1D& Axis = BlendSpace->GetAxis();
        NewSampleValue = (Axis.MinValue + Axis.MaxValue) * 0.5f;
        BlendInput = Axis.MinValue;

        if (!BlendSpace->GetSamples().IsEmpty())
        {
            Component->PlayBlendSpace(BlendSpace, BlendInput);
        }
        else
        {
            bPlaying = false;
        }
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

void FBlendSpaceEditorTab::DrawToolbar()
{
    if (!BlendSpace)
    {
        ImGui::TextUnformatted("No BlendSpace asset.");
        return;
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
        if (BlendSpace->AddSample(NewSampleValue, NewSampleAnimation, NewSampleRateScale))
        {
            bSamplesChanged = true;
            bDirty = true;
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
    }

    if (bSamplesChanged)
    {
        RefreshPreview(PreviousTime);
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
        const FString FilePath = PathUtils::ToUtf8(fs::path("Assets") / PathUtils::FromUtf8(BlendSpace->GetPath()));

        if (FJsonArchive::SaveBlendSpace(BlendSpace, FilePath))
        {
            bDirty = false;
        }
    }

    ImGui::EndDisabled();

    if (bDirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Unsaved changes");
    }
}