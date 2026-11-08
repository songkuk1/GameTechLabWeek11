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
    NewTransition.BlendDuration = 0.2f;

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

    DrawParameterEditor();

    ImGui::Separator();

    DrawTransitionEditor();

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

    const FString NewParameterLabel = NewBlendSpaceParameterName.IsNone() ? "None" : NewBlendSpaceParameterName.ToString();

    if (ImGui::BeginCombo("BlendSpace Float Parameter", NewParameterLabel.c_str()))
    {
        if (ImGui::Selectable("None", NewBlendSpaceParameterName.IsNone()))
        {
            NewBlendSpaceParameterName = NAME_None;
        }

        for (const FAnimGraphParameterDefinition& Parameter : Graph->GetParameters())
        {
            if (Parameter.Type != EAnimGraphParameterType::Float)
            {
                continue;
            }

            const bool bSelected = Parameter.Name == NewBlendSpaceParameterName;

            const FString Name = Parameter.Name.ToString();

            if (ImGui::Selectable(Name.c_str(), bSelected))
            {
                NewBlendSpaceParameterName = Parameter.Name;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::BeginDisabled(NewBlendSpace == nullptr);

    if (ImGui::Button("Add BlendSpace State"))
    {
        const FName StateName(NewStateNameBuffer);
        const bool bWasEmpty = Graph->GetStates().IsEmpty();

        if (Graph->AddBlendSpaceState(StateName, NewBlendSpace, NewBlendSpaceParameterName))
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
            SelectedTransitionIndex = -1;
            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
    }

    float PlayRate = State.PlayRate;
    bool bLooping = State.bLooping;

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
        FName SelectedBlendParameterName = State.BlendParameterName;
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

        const FString ParameterLabel = SelectedBlendParameterName.IsNone() ? "None" : SelectedBlendParameterName.ToString();

        if (ImGui::BeginCombo("BlendSpace Parameter", ParameterLabel.c_str()))
        {
            if (ImGui::Selectable("None", SelectedBlendParameterName.IsNone()))
            {
                SelectedBlendParameterName = NAME_None;
                bChanged = true;
            }

            for (const FAnimGraphParameterDefinition& Parameter : Graph->GetParameters())
            {
                if (Parameter.Type != EAnimGraphParameterType::Float)
                {
                    continue;
                }

                const bool bSelected = Parameter.Name == SelectedBlendParameterName;

                const FString Name = Parameter.Name.ToString();

                if (ImGui::Selectable(Name.c_str(), bSelected))
                {
                    SelectedBlendParameterName = Parameter.Name;
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
            if (Graph->SetBlendSpaceState(State.Name, SelectedBlendSpace, SelectedBlendParameterName, PlayRate, bLooping))
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

void FAnimationGraphEditorTab::DrawParameterEditor()
{
    ImGui::TextUnformatted("Parameters");

    const TArray<FAnimGraphParameterDefinition>& Parameters = Graph->GetParameters();

    ImGui::BeginChild("AnimationGraphParameterList", ImVec2(0.0f, 75.0f), true);

    for (int32 Index = 0; Index < Parameters.Num(); ++Index)
    {
        const FAnimGraphParameterDefinition& Parameter = Parameters[Index];

        const FString Name = Parameter.Name.ToString();

        const FString Label = Name + (Parameter.Type == EAnimGraphParameterType::Bool ? " [Bool]" : " [Float]");

        if (ImGui::Selectable(Label.c_str(), SelectedParameterIndex == Index))
        {
            SelectedParameterIndex = Index;

            std::snprintf(RenameParameterBuffer, sizeof(RenameParameterBuffer), "%s", Name.c_str());
        }
    }

    ImGui::EndChild();

    ImGui::TextUnformatted("Add Parameter");

    ImGui::InputText("New Parameter Name", NewParameterNameBuffer, sizeof(NewParameterNameBuffer));

    const char* ParameterTypeNames[] =
    {
        "Bool",
        "Float"
    };

    ImGui::Combo("Parameter Type", &NewParameterTypeIndex, ParameterTypeNames, IM_ARRAYSIZE(ParameterTypeNames));

    if (NewParameterTypeIndex == 0)
    {
        ImGui::Checkbox("Default Value", &bNewParameterBoolDefault);
    }
    else
    {
        ImGui::DragFloat("Default Value", &NewParameterFloatDefault, 0.1f);
    }

    if (ImGui::Button("Add Parameter"))
    {
        const FName NewName(NewParameterNameBuffer);

        bool bAdded = false;

        if (NewParameterTypeIndex == 0)
        {
            bAdded = Graph->AddBoolParameter(NewName, bNewParameterBoolDefault);
        }
        else
        {
            bAdded = Graph->AddFloatParameter(NewName, NewParameterFloatDefault);
        }

        if (bAdded)
        {
            SelectedParameterIndex = Graph->GetParameters().Num() - 1;

            std::snprintf(RenameParameterBuffer, sizeof(RenameParameterBuffer), "%s", NewName.ToString().c_str());

            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
        else
        {
            ErrorMessage = "Could not add Parameter. Check its name and value.";
        }
    }

    ImGui::Separator();

    if (!Parameters.IsValidIndex(SelectedParameterIndex))
    {
        ImGui::TextDisabled("Select a Parameter to edit.");
    }
    else
    {
        const FAnimGraphParameterDefinition Parameter = Parameters[SelectedParameterIndex];

        ImGui::Text("Selected Parameter: %s", Parameter.Name.ToString().c_str());

        ImGui::InputText("Rename Parameter", RenameParameterBuffer, sizeof(RenameParameterBuffer));

        bool bBoolDefault = Parameter.BoolDefaultValue;
        float FloatDefault = Parameter.FloatDefaultValue;

        if (Parameter.Type == EAnimGraphParameterType::Bool)
        {
            ImGui::Checkbox("Parameter Default", &bBoolDefault);
        }
        else
        {
            ImGui::DragFloat("Parameter Default", &FloatDefault, 0.1f);
        }

        if (ImGui::Button("Apply Parameter Changes"))
        {
            const FName NewName(RenameParameterBuffer);

            if (Graph->RenameParameter(Parameter.Name, NewName))
            {
                bool bUpdated = false;

                if (Parameter.Type == EAnimGraphParameterType::Bool)
                {
                    bUpdated = Graph->SetBoolParameterDefault(NewName, bBoolDefault);
                }
                else
                {
                    bUpdated = Graph->SetFloatParameterDefault(NewName, FloatDefault);
                }

                if (bUpdated)
                {
                    bDirty = true;
                    ErrorMessage.clear();

                    RefreshPreview();
                }
                else
                {
                    ErrorMessage = "Could not update Parameter default value.";
                }
            }
            else
            {
                ErrorMessage = "Could not rename Parameter. Check for duplicate names.";
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove Parameter"))
        {
            if (Graph->RemoveParameter(Parameter.Name))
            {
                SelectedParameterIndex = -1;

                bDirty = true;
                ErrorMessage.clear();

                RefreshPreview();
            }
            else
            {
                ErrorMessage = "Cannot remove Parameter while it is being used.";
            }
        }
    }

    ImGui::Separator();

    ImGui::TextUnformatted("Preview Parameter Values");

    USkeletalMeshComponent* Component = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;

    UAnimInstance* AnimInstance = Component ? Component->GetAnimInstance() : nullptr;

    if (!AnimInstance)
    {
        ImGui::TextDisabled("Animation preview is not available.");
        return;
    }

    const TArray<FAnimGraphParameterDefinition>& CurrentParameters = Graph->GetParameters();

    for (int32 Index = 0; Index < CurrentParameters.Num(); ++Index)
    {
        const FAnimGraphParameterDefinition& Parameter = CurrentParameters[Index];

        ImGui::PushID(Index);

        const FString Name = Parameter.Name.ToString();

        if (Parameter.Type == EAnimGraphParameterType::Bool)
        {
            bool Value = AnimInstance->GetBoolParameter(Parameter.Name);

            if (ImGui::Checkbox(Name.c_str(), &Value))
            {
                AnimInstance->SetBoolParameter(Parameter.Name, Value);

                Component->TickAnimation(0.0f);
            }
        }
        else
        {
            float Value = AnimInstance->GetFloatParameter(Parameter.Name);

            if (ImGui::DragFloat( Name.c_str(), &Value, 0.1f))
            {
                AnimInstance->SetFloatParameter(Parameter.Name, Value);
                Component->TickAnimation(0.0f);
            }
        }

        ImGui::PopID();
    }
}

void FAnimationGraphEditorTab::DrawTransitionEditor()
{
    ImGui::TextUnformatted("Transitions");

    const TArray<FAnimGraphStateDefinition>& States = Graph->GetStates();

    const TArray<FAnimGraphParameterDefinition>& Parameters = Graph->GetParameters();

    const EAnimTransitionRule Rules[] =
    {
        EAnimTransitionRule::Always,
        EAnimTransitionRule::BoolTrue,
        EAnimTransitionRule::BoolFalse,
        EAnimTransitionRule::StateFinished,
        EAnimTransitionRule::FloatGreaterThan,
        EAnimTransitionRule::FloatLessThan,
        EAnimTransitionRule::FloatGreaterOrEqual,
        EAnimTransitionRule::FloatLessOrEqual
    };

    const char* RuleNames[] =
    {
        "Always",
        "Bool True",
        "Bool False",
        "State Finished",
        "Float Greater Than",
        "Float Less Than",
        "Float Greater Or Equal",
        "Float Less Or Equal"
    };

    const auto IsBoolRule = [](EAnimTransitionRule Rule)
        {
            return Rule == EAnimTransitionRule::BoolTrue ||
                Rule == EAnimTransitionRule::BoolFalse;
        };

    const auto IsFloatRule = [](EAnimTransitionRule Rule)
        {
            return Rule == EAnimTransitionRule::FloatGreaterThan ||
                Rule == EAnimTransitionRule::FloatLessThan ||
                Rule == EAnimTransitionRule::FloatGreaterOrEqual ||
                Rule == EAnimTransitionRule::FloatLessOrEqual;
        };

    const auto EnsureParameter = [&](FAnimGraphTransitionDefinition& Transition)
        {
            EAnimGraphParameterType RequiredType;

            if (IsBoolRule(Transition.Rule))
            {
                RequiredType = EAnimGraphParameterType::Bool;
            }
            else if (IsFloatRule(Transition.Rule))
            {
                RequiredType = EAnimGraphParameterType::Float;
            }
            else
            {
                Transition.ParameterName = NAME_None;
                return;
            }

            bool bFound = false;

            for (const FAnimGraphParameterDefinition& Parameter : Parameters)
            {
                if (Parameter.Name == Transition.ParameterName &&
                    Parameter.Type == RequiredType)
                {
                    bFound = true;
                    break;
                }
            }

            if (!bFound)
            {
                Transition.ParameterName = NAME_None;

                for (const FAnimGraphParameterDefinition& Parameter : Parameters)
                {
                    if (Parameter.Type == RequiredType)
                    {
                        Transition.ParameterName = Parameter.Name;
                        break;
                    }
                }
            }
        };

    const auto DrawParameterCombo = [&](const char* Label, FAnimGraphTransitionDefinition& Transition)
        {
            EnsureParameter(Transition);

            const FString CurrentLabel = Transition.ParameterName.IsNone() ? "Select Parameter" : Transition.ParameterName.ToString();

            const EAnimGraphParameterType RequiredType = IsBoolRule(Transition.Rule) ? EAnimGraphParameterType::Bool : EAnimGraphParameterType::Float;

            bool bHasParameter = false;

            for (const FAnimGraphParameterDefinition& Parameter : Parameters)
            {
                if (Parameter.Type == RequiredType)
                {
                    bHasParameter = true;
                    break;
                }
            }

            if (!bHasParameter)
            {
                ImGui::TextDisabled(RequiredType == EAnimGraphParameterType::Bool ? "Add a Bool Parameter first." : "Add a Float Parameter first.");

                return;
            }

            if (ImGui::BeginCombo(Label, CurrentLabel.c_str()))
            {
                for (const FAnimGraphParameterDefinition& Parameter : Parameters)
                {
                    if (Parameter.Type != RequiredType)
                    {
                        continue;
                    }

                    const bool bSelected = Parameter.Name == Transition.ParameterName;

                    const FString Name = Parameter.Name.ToString();

                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        Transition.ParameterName = Parameter.Name;
                    }
                }

                ImGui::EndCombo();
            }
        };

    const auto DrawRuleCombo = [&](const char* Label, FAnimGraphTransitionDefinition& Transition)
        {
            int32 RuleIndex = static_cast<int32>(Transition.Rule);

            if (RuleIndex < 0 || RuleIndex >= IM_ARRAYSIZE(Rules))
            {
                RuleIndex = 0;
            }

            if (ImGui::BeginCombo(Label, RuleNames[RuleIndex]))
            {
                for (int32 Index = 0; Index < IM_ARRAYSIZE(Rules); ++Index)
                {
                    const bool bSelected = RuleIndex == Index;

                    if (ImGui::Selectable(RuleNames[Index], bSelected))
                    {
                        Transition.Rule = Rules[Index];
                        EnsureParameter(Transition);
                    }
                }

                ImGui::EndCombo();
            }
        };

    const auto DrawStateCombo = [&](const char* Label, FName& SelectedName)
        {
            const FString CurrentLabel = SelectedName.IsNone() ? "Select State" : SelectedName.ToString();

            if (ImGui::BeginCombo(Label, CurrentLabel.c_str()))
            {
                for (const FAnimGraphStateDefinition& State : States)
                {
                    const bool bSelected = State.Name == SelectedName;
                    const FString Name = State.Name.ToString();

                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        SelectedName = State.Name;
                    }
                }

                ImGui::EndCombo();
            }
        };

    ImGui::TextUnformatted("Add Transition");

    if (!NewTransition.bAnyState)
    {
        if (NewTransition.FromState.IsNone() && States.Num() >= 1)
        {
            NewTransition.FromState = States[0].Name;
        }

        if (NewTransition.ToState.IsNone() && States.Num() >= 2)
        {
            NewTransition.ToState = States[1].Name;
        }
    }
    else
    {
        NewTransition.FromState = NAME_None;
    }

    ImGui::Checkbox("Any State Transition", &NewTransition.bAnyState);

    if (NewTransition.bAnyState)
    {
        NewTransition.FromState = NAME_None;
    }
    else
    {
        DrawStateCombo("From State", NewTransition.FromState);
    }

    DrawStateCombo("To State", NewTransition.ToState);

    DrawRuleCombo("Rule", NewTransition);

    if (IsBoolRule(NewTransition.Rule) || IsFloatRule(NewTransition.Rule))
    {
        DrawParameterCombo("Parameter", NewTransition);
    }
    else
    {
        NewTransition.ParameterName = NAME_None;
    }

    if (IsFloatRule(NewTransition.Rule))
    {
        ImGui::DragFloat("Float Threshold", &NewTransition.FloatThreshold, 1.0f);
    }

    ImGui::InputInt("Priority", &NewTransition.Priority);

    ImGui::DragFloat("Blend Duration", &NewTransition.BlendDuration, 0.01f, 0.0f, 10.0f);

    if (ImGui::Button("Add Transition"))
    {
        bool bAdded = false;

        if (NewTransition.bAnyState)
        {
            bAdded = Graph->AddAnyStateTransition(
                NewTransition.ToState,
                NewTransition.Rule,
                NewTransition.ParameterName,
                NewTransition.Priority,
                NewTransition.BlendDuration,
                NewTransition.FloatThreshold);
        }
        else
        {
            bAdded = Graph->AddTransition(
                NewTransition.FromState,
                NewTransition.ToState,
                NewTransition.Rule,
                NewTransition.ParameterName,
                NewTransition.Priority,
                NewTransition.BlendDuration,
                NewTransition.FloatThreshold);
        }

        if (bAdded)
        {
            SelectedTransitionIndex = Graph->GetTransitions().Num() - 1;

            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
        else
        {
            ErrorMessage = "Could not add Transition. Check State and Parameter settings.";
        }
    }

    ImGui::Separator();

    const TArray<FAnimGraphTransitionDefinition>& Transitions =
        Graph->GetTransitions();

    ImGui::BeginChild(
        "AnimationGraphTransitionList",
        ImVec2(0.0f, 85.0f),
        true);

    for (int32 Index = 0; Index < Transitions.Num(); ++Index)
    {
        const FAnimGraphTransitionDefinition& Transition =
            Transitions[Index];

        const FString FromLabel = Transition.bAnyState
            ? "Any State"
            : Transition.FromState.ToString();

        const FString ToLabel = Transition.ToState.ToString();

        const FString Label =
            FromLabel + " -> " + ToLabel;

        if (ImGui::Selectable(
            Label.c_str(),
            SelectedTransitionIndex == Index))
        {
            SelectedTransitionIndex = Index;
        }
    }

    ImGui::EndChild();

    if (!Transitions.IsValidIndex(SelectedTransitionIndex))
    {
        ImGui::TextDisabled("Select a Transition to edit.");
        return;
    }

    FAnimGraphTransitionDefinition EditableTransition =
        Transitions[SelectedTransitionIndex];

    ImGui::Separator();

    ImGui::Text(
        "Selected Transition: %s -> %s",
        EditableTransition.bAnyState
        ? "Any State"
        : EditableTransition.FromState.ToString().c_str(),
        EditableTransition.ToState.ToString().c_str());

    ImGui::Checkbox(
        "Edit Any State",
        &EditableTransition.bAnyState);

    if (EditableTransition.bAnyState)
    {
        EditableTransition.FromState = NAME_None;
    }
    else
    {
        DrawStateCombo(
            "Edit From State",
            EditableTransition.FromState);
    }

    DrawStateCombo(
        "Edit To State",
        EditableTransition.ToState);

    DrawRuleCombo(
        "Edit Rule",
        EditableTransition);

    if (IsBoolRule(EditableTransition.Rule) ||
        IsFloatRule(EditableTransition.Rule))
    {
        DrawParameterCombo(
            "Edit Parameter",
            EditableTransition);
    }
    else
    {
        EditableTransition.ParameterName = NAME_None;
    }

    if (IsFloatRule(EditableTransition.Rule))
    {
        ImGui::DragFloat(
            "Edit Float Threshold",
            &EditableTransition.FloatThreshold,
            1.0f);
    }

    ImGui::InputInt(
        "Edit Priority",
        &EditableTransition.Priority);

    ImGui::DragFloat(
        "Edit Blend Duration",
        &EditableTransition.BlendDuration,
        0.01f,
        0.0f,
        10.0f);

    if (ImGui::Button("Apply Transition Changes"))
    {
        if (Graph->SetTransition(
            SelectedTransitionIndex,
            EditableTransition))
        {
            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
        else
        {
            ErrorMessage =
                "Could not update Transition. Check the selected settings.";
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove Transition"))
    {
        if (Graph->RemoveTransition(SelectedTransitionIndex))
        {
            SelectedTransitionIndex = -1;

            bDirty = true;
            ErrorMessage.clear();

            RefreshPreview();
        }
    }
}
