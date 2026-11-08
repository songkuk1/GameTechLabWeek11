#include "EnginePCH.h"
#include "AnimationGraphEditorTab.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"

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

#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr float FramingPitchDegrees = 20.0f;

    FEditorTabSettings MakeAnimationGraphEditorSettings(const UAnimationGraph* Graph)
    {
        const FString AssetPath = Graph ? Graph->GetPath() : "Unknown";

        const size_t NameStart = AssetPath.find_last_of("/\\");

        FString AssetName = NameStart == FString::npos ? AssetPath : AssetPath.substr(NameStart + 1);

        const size_t ExtensionStart = AssetName.rfind('.');

        if (ExtensionStart != FString::npos)
        {
            AssetName.resize(ExtensionStart);
        }

        FEditorTabSettings Settings;
        Settings.Title = "Animation Graph: " + AssetName;
        Settings.DockSpaceName = "AnimationGraphEditorDock_" + AssetPath;

        Settings.bClosable = true;
        Settings.bIsolatePanels = true;

        return Settings;
    }
}

FAnimationGraphEditorTab::FAnimationGraphEditorTab(UAnimationGraph* InGraph) : FEditorTab(MakeAnimationGraphEditorSettings(InGraph)) , Graph(InGraph)
{
    if (!Graph)
    {
        return;
    }

    PreviewScene = MakeUnique<FPreviewScene>();

    for (TObjectIterator<USkeletalMesh> It; It; ++It)
    {
        USkeletalMesh* Candidate = *It;

        if (Candidate && Candidate->GetSkeleton() == Graph->GetSkeleton())
        {
            SkeletalMesh = Candidate;
            break;
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

        if (Animation && Animation->GetSkeleton() == Graph->GetSkeleton())
        {
            AvailableAnimations.Add(Animation);
        }
    }

    for (TObjectIterator<UBlendSpace1D> It; It; ++It)
    {
        UBlendSpace1D* BlendSpace = *It;

        if (BlendSpace && BlendSpace->GetSkeleton() == Graph->GetSkeleton())
        {
            AvailableBlendSpaces.Add(BlendSpace);
        }
    }

    std::sort(AvailableAnimations.begin(), AvailableAnimations.end(), [](const UAnimSequence* A, const UAnimSequence* B) { return A->GetPath() < B->GetPath(); });

    std::sort(AvailableBlendSpaces.begin(), AvailableBlendSpaces.end(), [](const UBlendSpace1D* A, const UBlendSpace1D* B) { return A->GetPath() < B->GetPath(); });

    if (!AvailableAnimations.IsEmpty())
    {
        NewSequence = AvailableAnimations[0];
    }

    if (!AvailableBlendSpaces.IsEmpty())
    {
        NewBlendSpace = AvailableBlendSpaces[0];
    }

    if (PreviewActor && Graph->IsAnimationGraphDataValid())
    {
        RefreshPreview();
    }

    ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("AnimationGraphViewport##") + Graph->GetPath(), [this]() { DrawToolbar(); });
}

FAnimationGraphEditorTab::~FAnimationGraphEditorTab()
{
    Panels.Empty();

    ViewportPanel = nullptr;
    PreviewActor = nullptr;
}

UWorld* FAnimationGraphEditorTab::GetWorld() const
{
    return PreviewScene ? PreviewScene->GetWorld() : nullptr;
}

void FAnimationGraphEditorTab::Tick(float DeltaTime)
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

void FAnimationGraphEditorTab::RouteInput(const FViewportInput& BaseInput)
{
    if (ViewportPanel)
    {
        ViewportPanel->RouteInput(BaseInput);
    }
}

void FAnimationGraphEditorTab::CollectViews(TArray<FEditorTabView>& OutViews)
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

void FAnimationGraphEditorTab::FrameMesh()
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

void FAnimationGraphEditorTab::RefreshPreview()
{
    if (!PreviewActor)
    {
        bPlaying = false;
        return;
    }

    USkeletalMeshComponent* Component = PreviewActor->GetSkeletalMeshComponent();

    if (!Graph->IsAnimationGraphDataValid())
    {
        Component->PlayAnimation(nullptr);
        bPlaying = false;
        return;
    }

    bPlaying = Component->SetAnimationGraph(Graph);

    if (!bPlaying)
    {
        ErrorMessage = "Could not initialize Animation Graph preview.";
    }
    else
    {
        ErrorMessage.clear();
    }
}

bool FAnimationGraphEditorTab::CanCloseNow()
{
    if (!bDirty)
    {
        return true;
    }

    bShowClosePopup = true;
    return false;
}

bool FAnimationGraphEditorTab::SaveAnimationGraphAsset()
{
    if (!Graph || Graph->GetPath().empty())
    {
        ErrorMessage = "Invalid Animation Graph asset path.";
        return false;
    }

    if (!Graph->IsAnimationGraphDataValid())
    {
        ErrorMessage = "Animation Graph data is invalid.";
        return false;
    }

    const std::filesystem::path FilePath = std::filesystem::path("Assets") / PathUtils::FromUtf8(Graph->GetPath());

    if (!FJsonArchive::SaveAnimationGraph(Graph, PathUtils::ToUtf8(FilePath)))
    {
        ErrorMessage = "Could not save Animation Graph.";
        return false;
    }

    bDirty = false;
    ErrorMessage.clear();

    return true;
}

void FAnimationGraphEditorTab::DrawToolbar()
{
    if (!Graph)
    {
        ImGui::TextUnformatted("No Animation Graph asset.");
        return;
    }

    if (bShowClosePopup)
    {
        ImGui::OpenPopup("Unsaved Animation Graph Changes");
        bShowClosePopup = false;
    }

    if (ImGui::BeginPopupModal("Unsaved Animation Graph Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("There are unsaved changes.");

        if (!ErrorMessage.empty())
        {
            ImGui::TextUnformatted(ErrorMessage.c_str());
        }

        if (ImGui::Button("Save"))
        {
            if (SaveAnimationGraphAsset())
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

    ImGui::TextUnformatted("Animation Graph");

    ImGui::Text("Skeleton: %s", Graph->GetSkeleton() ? Graph->GetSkeleton()->GetPath().c_str() : "None");

    ImGui::SameLine();

    ImGui::BeginDisabled(!bDirty);

    if (ImGui::Button("Save Graph"))
    {
        SaveAnimationGraphAsset();
    }

    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!Graph->IsAnimationGraphDataValid());

    if (ImGui::Button(bPlaying ? "Pause" : "Play"))
    {
        if (bPlaying)
        {
            bPlaying = false;
        }
        else
        {
            RefreshPreview();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Restart"))
    {
        RefreshPreview();
    }

    ImGui::EndDisabled();

    if (bDirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Unsaved changes");
    }

    ImGui::Separator();

    DrawStateEditor();

    ImGui::Separator();

    ImGui::Text("Parameters: %d | Transitions: %d", Graph->GetParameters().Num(), Graph->GetTransitions().Num());

    if (!ErrorMessage.empty())
    {
        ImGui::TextUnformatted(ErrorMessage.c_str());
    }

    if (!SkeletalMesh || !PreviewActor)
    {
        ImGui::TextDisabled("No compatible Skeletal Mesh is available for preview.");
    }
}

void FAnimationGraphEditorTab::DrawStateEditor()
{
    ImGui::TextUnformatted("States");

    const TArray<FAnimGraphStateDefinition>& States = Graph->GetStates();

    ImGui::BeginChild("AnimationGraphStateList", ImVec2(0.0f, 90.0f), true);

    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        const FAnimGraphStateDefinition& State = States[Index];

        const FString StateName = State.Name.ToString();

        if (ImGui::Selectable(StateName.c_str(), SelectedStateIndex == Index))
        {
            SelectedStateIndex = Index;

            std::snprintf(RenameStateBuffer, sizeof(RenameStateBuffer), "%s", StateName.c_str());
        }
    }

    ImGui::EndChild();

    const FName EntryState = Graph->GetEntryState();
    const FString EntryLabel = EntryState.IsNone() ? "None" : EntryState.ToString();

    if (ImGui::BeginCombo("Entry State", EntryLabel.c_str()))
    {
        for (int32 Index = 0; Index < States.Num(); ++Index)
        {
            const FAnimGraphStateDefinition& State = States[Index];

            const bool bSelected = State.Name == EntryState;
            const FString StateName = State.Name.ToString();

            if (ImGui::Selectable(StateName.c_str(), bSelected))
            {
                if (Graph->SetEntryState(State.Name))
                {
                    bDirty = true;
                    RefreshPreview();
                }
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::TextUnformatted("Add Sequence State");

    ImGui::InputText("New State Name", NewStateNameBuffer, sizeof(NewStateNameBuffer));

    const FString SequenceLabel = NewSequence ? NewSequence->GetPath() : "Select Animation";

    if (ImGui::BeginCombo(
        "Sequence",
        SequenceLabel.c_str()))
    {
        for (UAnimSequence* Animation : AvailableAnimations)
        {
            const bool bSelected = Animation == NewSequence;

            if (ImGui::Selectable(Animation->GetPath().c_str(), bSelected))
            {
                NewSequence = Animation;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::BeginDisabled(NewSequence == nullptr);

    if (ImGui::Button("Add Sequence State"))
    {
        const FName StateName(NewStateNameBuffer);
        const bool bWasEmpty = Graph->GetStates().IsEmpty();

        if (Graph->AddSequenceState(StateName, NewSequence))
        {
            if (bWasEmpty)
            {
                Graph->SetEntryState(StateName);
            }

            SelectedStateIndex = Graph->GetStates().Num() - 1;

            std::snprintf(RenameStateBuffer, sizeof(RenameStateBuffer), "%s", StateName.ToString().c_str());

            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
        else
        {
            ErrorMessage = "Could not add Sequence State. Check its name and Skeleton.";
        }
    }

    ImGui::EndDisabled();

    const FString BlendSpaceLabel = NewBlendSpace ? NewBlendSpace->GetPath() : "Select BlendSpace";

    if (ImGui::BeginCombo("BlendSpace", BlendSpaceLabel.c_str()))
    {
        for (UBlendSpace1D* BlendSpace : AvailableBlendSpaces)
        {
            const bool bSelected = BlendSpace == NewBlendSpace;

            if (ImGui::Selectable( BlendSpace->GetPath().c_str(), bSelected))
            {
                NewBlendSpace = BlendSpace;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::BeginDisabled(NewBlendSpace == nullptr);

    if (ImGui::Button("Add BlendSpace State"))
    {
        const FName StateName(NewStateNameBuffer);
        const bool bWasEmpty = Graph->GetStates().IsEmpty();

        if (Graph->AddBlendSpaceState(StateName, NewBlendSpace))
        {
            if (bWasEmpty)
            {
                Graph->SetEntryState(StateName);
            }

            SelectedStateIndex = Graph->GetStates().Num() - 1;

            std::snprintf(RenameStateBuffer, sizeof(RenameStateBuffer), "%s", StateName.ToString().c_str());

            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
        else
        {
            ErrorMessage = "Could not add BlendSpace State. Check its name and Skeleton.";
        }
    }

    ImGui::EndDisabled();

    ImGui::Separator();

    if (!States.IsValidIndex(SelectedStateIndex))
    {
        ImGui::TextDisabled("Select a State to edit.");
        return;
    }

    const FAnimGraphStateDefinition State = States[SelectedStateIndex];

    ImGui::Text("Selected State: %s", State.Name.ToString().c_str());

    ImGui::InputText("Rename", RenameStateBuffer, sizeof(RenameStateBuffer));

    if (ImGui::Button("Rename State"))
    {
        if (Graph->RenameState(State.Name, FName(RenameStateBuffer)))
        {
            bDirty = true;
            ErrorMessage.clear();
        }
        else
        {
            ErrorMessage = "Could not rename State.";
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove State"))
    {
        if (Graph->RemoveState(State.Name))
        {
            SelectedStateIndex = -1;
            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
    }

    float PlayRate = State.PlayRate;
    bool bLooping = State.bLooping;

    ImGui::DragFloat("Play Rate", &PlayRate, 0.01f);

    ImGui::Checkbox("Looping", &bLooping);

    if (State.NodeType == EAnimGraphStateNodeType::SequencePlayer)
    {
        UAnimSequence* SelectedAnimation = State.Sequence;

        const FString CurrentLabel = SelectedAnimation ? SelectedAnimation->GetPath() : "None";

        bool bChanged = false;

        if (ImGui::BeginCombo("State Animation", CurrentLabel.c_str()))
        {
            for (UAnimSequence* Animation : AvailableAnimations)
            {
                const bool bSelected = Animation == SelectedAnimation;

                if (ImGui::Selectable(Animation->GetPath().c_str(), bSelected))
                {
                    SelectedAnimation = Animation;
                    bChanged = true;
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::DragFloat("Play Rate", &PlayRate, 0.01f))
        {
            bChanged = true;
        }

        if (ImGui::Checkbox("Looping", &bLooping))
        {
            bChanged = true;
        }

        if (bChanged)
        {
            if (Graph->SetSequenceState(State.Name, SelectedAnimation, PlayRate, bLooping))
            {
                bDirty = true;
                ErrorMessage.clear();
                RefreshPreview();
            }
            else
            {
                ErrorMessage = "Could not update Sequence State.";
            }
        }
    }
    else if (State.NodeType == EAnimGraphStateNodeType::BlendSpacePlayer)
    {
        UBlendSpace1D* SelectedBlendSpace = State.BlendSpace;

        const FString CurrentLabel = SelectedBlendSpace ? SelectedBlendSpace->GetPath() : "None";

        bool bChanged = false;

        if (ImGui::BeginCombo("State BlendSpace", CurrentLabel.c_str()))
        {
            for (UBlendSpace1D* BlendSpace : AvailableBlendSpaces)
            {
                const bool bSelected = BlendSpace == SelectedBlendSpace;

                if (ImGui::Selectable(BlendSpace->GetPath().c_str(), bSelected))
                {
                    SelectedBlendSpace = BlendSpace;
                    bChanged = true;
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::DragFloat("Play Rate", &PlayRate, 0.01f))
        {
            bChanged = true;
        }

        if (ImGui::Checkbox("Looping", &bLooping))
        {
            bChanged = true;
        }

        if (bChanged)
        {
            if (Graph->SetBlendSpaceState(State.Name, SelectedBlendSpace, State.BlendParameterName, PlayRate, bLooping))
            {
                bDirty = true;
                ErrorMessage.clear();
                RefreshPreview();
            }
            else
            {
                ErrorMessage = "Could not update BlendSpace State.";
            }
        }
    }
}