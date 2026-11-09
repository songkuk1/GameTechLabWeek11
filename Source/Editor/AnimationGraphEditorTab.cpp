#include "EnginePCH.h"
#include "AnimationGraphEditorTab.h"
#include "imgui_node_editor.h"

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
#include <limits>

namespace ed = ax::NodeEditor;

class FAnimationGraphEditorPanel : public IEditorPanel
{
public:
    FAnimationGraphEditorPanel(FString InPanelName, std::function<void()> InDrawContent) : PanelName(std::move(InPanelName)), DrawContent(std::move(InDrawContent)){}
    bool Init() override { return true; }
    void Tick(float DeltaTime) override { (void)DeltaTime; }
    void OnRender() override
    {
        const bool bVisible = ImGui::Begin(PanelName.c_str());
        if (bVisible && DrawContent)
        {
            DrawContent();
        }
        ImGui::End();
    }
    const char* GetPanelName() const override { return PanelName.c_str(); }
private:
    FString PanelName;
    std::function<void()> DrawContent;
};

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
        Settings.DockSpaceName = "AnimationGraphWorkspaceV2_" + AssetPath;

        Settings.bClosable = true;
        Settings.bIsolatePanels = true;

        return Settings;
    }

    std::uint64_t HashNodeEditorIdentifier(const FString& Identifier)
    {
        std::uint64_t Hash = 14695981039346656037ull;

        for (const unsigned char Character : Identifier)
        {
            Hash ^= Character;
            Hash *= 1099511628211ull;
        }

        return Hash == 0 ? 1 : Hash;
    }

    ed::NodeId MakeStateNodeId(const FName& StateName)
    {
        FString Identifier = "StateNode:";
        Identifier += StateName.ToString();
        return ed::NodeId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::NodeId MakeAnyStateNodeId()
    {
        return ed::NodeId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier("AnyStateNode")));
    }

    enum EGraphPinSide : int32
    {
        GraphPinSideLeft = 0,
        GraphPinSideRight,
        GraphPinSideTop,
        GraphPinSideBottom
    };

    constexpr int32 GraphPinSideCount = 4;

    const char* GetGraphPinSideName(int32 Side)
    {
        static const char* Names[] = { "Left", "Right", "Top", "Bottom" };
        return Side >= 0 && Side < GraphPinSideCount ? Names[Side] : "Unknown";
    }

    ed::PinId MakeStateInputPinId(const FName& StateName, int32 Side)
    {
        FString Identifier = "StateInput:";
        Identifier += StateName.ToString();
        Identifier += ":";
        Identifier += GetGraphPinSideName(Side);
        return ed::PinId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::PinId MakeStateOutputPinId(const FName& StateName, int32 Side)
    {
        FString Identifier = "StateOutput:";
        Identifier += StateName.ToString();
        Identifier += ":";
        Identifier += GetGraphPinSideName(Side);
        return ed::PinId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::PinId MakeAnyStateOutputPinId(int32 Side)
    {
        FString Identifier = "AnyStateOutput:";
        Identifier += GetGraphPinSideName(Side);
        return ed::PinId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::PinId MakeEntryOutputPinId(int32 Side)
    {
        FString Identifier = "EntryOutput:";
        Identifier += GetGraphPinSideName(Side);
        return ed::PinId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::LinkId MakeTransitionLinkId(const FAnimGraphTransitionDefinition& Transition)
    {
        FString Identifier = Transition.bAnyState ? "AnyStateTransition:" : "Transition:";

        if (!Transition.bAnyState)
        {
            Identifier += Transition.FromState.ToString();
            Identifier += "->";
        }

        Identifier += Transition.ToState.ToString();

        return ed::LinkId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::PinId MakeTransitionSourcePinId(const FAnimGraphTransitionDefinition& Transition)
    {
        FString Identifier = Transition.bAnyState ? "AnyStateSourcePin:" : "StateSourcePin:";
        if (!Transition.bAnyState)
        {
            Identifier += Transition.FromState.ToString();
            Identifier += "->";
        }
        Identifier += Transition.ToState.ToString();
        return ed::PinId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::PinId MakeTransitionDestinationPinId(const FAnimGraphTransitionDefinition& Transition)
    {
        FString Identifier = "TransitionDestinationPin:";
        Identifier += Transition.bAnyState ? "AnyState" : Transition.FromState.ToString();
        Identifier += "->";
        Identifier += Transition.ToState.ToString();
        return ed::PinId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier(Identifier)));
    }

    ed::NodeId MakeEntryNodeId()
    {
        return ed::NodeId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier("EntryNode")));
    }

    ed::LinkId MakeEntryLinkId()
    {
        return ed::LinkId(static_cast<std::uintptr_t>(HashNodeEditorIdentifier("EntryToDefaultState")));
    }

    FString FormatTransitionFloat(float Value)
    {
        char Buffer[32];
        std::snprintf(Buffer, sizeof(Buffer), "%.3f", Value);
        FString Result = Buffer;
        while (!Result.empty() && Result.back() == '0')
        {
            Result.pop_back();
        }
        if (!Result.empty() && Result.back() == '.')
        {
            Result.pop_back();
        }
        return Result;
    }

    FString GetTransitionConditionLabel(const FAnimGraphTransitionDefinition& Transition)
    {
        const FString ParameterName = Transition.ParameterName.IsNone() ? "?" : Transition.ParameterName.ToString();
        switch (Transition.Rule)
        {
        case EAnimTransitionRule::Always:
            return "Always";

        case EAnimTransitionRule::StateFinished:
            return "State Finished";

        case EAnimTransitionRule::BoolTrue:
            return ParameterName + " = true";

        case EAnimTransitionRule::BoolFalse:
            return ParameterName + " = false";

        case EAnimTransitionRule::FloatGreaterThan:
            return ParameterName + " > " + FormatTransitionFloat(Transition.FloatThreshold);

        case EAnimTransitionRule::FloatLessThan:
            return ParameterName + " < " + FormatTransitionFloat(Transition.FloatThreshold);

        case EAnimTransitionRule::FloatGreaterOrEqual:
            return ParameterName + " >= " + FormatTransitionFloat(Transition.FloatThreshold);

        case EAnimTransitionRule::FloatLessOrEqual:
            return ParameterName + " <= " + FormatTransitionFloat(Transition.FloatThreshold);
        }

        return "Unknown";
    }
}

FAnimationGraphEditorTab::FAnimationGraphEditorTab(UAnimationGraph* InGraph) : FEditorTab(MakeAnimationGraphEditorSettings(InGraph)) , Graph(InGraph)
{
    if (!Graph)
    {
        return;
    }

    ed::Config NodeEditorConfig;
    NodeEditorConfig.SettingsFile = nullptr;
    NodeEditorContext = ed::CreateEditor(&NodeEditorConfig);

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

    GraphEditorPanel = AddPanel<FAnimationGraphEditorPanel>(FString("Animation Graph Editor##") + Graph->GetPath(), [this]() { DrawGraphEditor(); });
    ViewportPanel = AddPanel<FPreviewViewportPanel>(GetWorld(), FString("AnimationGraphPreview##") + Graph->GetPath(), [this]() { DrawToolbar(); }, 90.0f);
}

FAnimationGraphEditorTab::~FAnimationGraphEditorTab()
{
    if (NodeEditorContext)
    {
        ed::DestroyEditor(NodeEditorContext);
        NodeEditorContext = nullptr;
    }

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

void FAnimationGraphEditorTab::BuildDefaultLayout(ImGuiID DockSpaceRoot)
{
    if (!GraphEditorPanel || !ViewportPanel)
    {
        FEditorTab::BuildDefaultLayout(DockSpaceRoot);
        return;
    }

    ImGuiID PreviewDockId = 0;
    ImGuiID GraphDockId = 0;

    ImGui::DockBuilderSplitNode(DockSpaceRoot, ImGuiDir_Right, 0.36f, &PreviewDockId, &GraphDockId);

    ImGui::DockBuilderDockWindow(GraphEditorPanel->GetPanelName(), GraphDockId);

    ImGui::DockBuilderDockWindow(ViewportPanel->GetPanelName(), PreviewDockId);
}

void FAnimationGraphEditorTab::DrawToolbar()
{
    ImGui::TextUnformatted("Animation Preview");

    ImGui::SameLine();

    ImGui::BeginDisabled(!PreviewActor);

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

    if (!ErrorMessage.empty())
    {
        ImGui::TextUnformatted(ErrorMessage.c_str());
    }
}

void FAnimationGraphEditorTab::DrawStateEditor()
{
    const TArray<FAnimGraphStateDefinition>& States = Graph->GetStates();

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

void FAnimationGraphEditorTab::DrawRuntimeDebug()
{
    if (!ImGui::CollapsingHeader("Runtime Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    USkeletalMeshComponent* Component = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;

    UAnimInstance* AnimInstance = Component ? Component->GetAnimInstance() : nullptr;

    if (!AnimInstance)
    {
        ImGui::TextDisabled("Animation Instance is not available.");
        return;
    }

    FAnimStateMachineDebugInfo DebugInfo;

    if (!AnimInstance->GetStateMachineDebugInfo(DebugInfo))
    {
        ImGui::TextDisabled("State Machine is not the active root node.");
        return;
    }

    // 현재 State
    const FString CurrentStateName = DebugInfo.CurrentStateName.IsNone() ? "None" : DebugInfo.CurrentStateName.ToString();

    ImGui::Text("Current State: %s", CurrentStateName.c_str());

    ImGui::Text("State Finished: %s", DebugInfo.bCurrentStateFinished ? "True" : "False");

    ImGui::Text("Playing: %s", bPlaying ? "True" : "False");

    ImGui::Separator();

    ImGui::TextUnformatted("Last Transition");

    if (!DebugInfo.bHasLastTransition)
    {
        ImGui::TextDisabled("No Transition has been triggered.");
    }
    else
    {
        const FString FromState = DebugInfo.bLastTransitionWasAnyState ? "Any State" : DebugInfo.LastTransitionFromState.ToString();

        const FString ToState = DebugInfo.LastTransitionToState.ToString();

        ImGui::Text("%s -> %s", FromState.c_str(), ToState.c_str());

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

        const int32 RuleIndex = static_cast<int32>(DebugInfo.LastTransitionRule);

        if (RuleIndex >= 0 && RuleIndex < IM_ARRAYSIZE(RuleNames))
        {
            ImGui::Text("Rule: %s", RuleNames[RuleIndex]);
        }
    }

    ImGui::Separator();

    // 현재 블렌딩 정보
    ImGui::Text("Blending: %s", DebugInfo.bIsBlending ? "True" : "False");

    ImGui::Text("Blend Alpha: %.3f", DebugInfo.TransitionAlpha);

    ImGui::Text("Blend Time: %.3f / %.3f sec", DebugInfo.TransitionElapsedTime, DebugInfo.TransitionDuration);

    ImGui::Text("Blend Progress: %.1f%%", DebugInfo.TransitionAlpha * 100.0f);
}

void FAnimationGraphEditorTab::DrawGraphEditor()
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

    ImGui::Text("Animation Graph: %s", Graph->GetPath().c_str());

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
        ImGui::TextDisabled("Unsaved");
    }

    ImGui::Separator();

    const ImVec2 Available = ImGui::GetContentRegionAvail();

    const float DetailsWidth = std::min(460.0f, std::max(300.0f, Available.x * 0.35f));

    const float CanvasWidth = std::max(240.0f, Available.x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x);

    ImGui::BeginChild("AnimationGraphCanvasRegion", ImVec2(CanvasWidth, 0.0f), true);

    DrawGraphCanvas();

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("AnimationGraphInspectorRegion", ImVec2(0.0f, 0.0f), true);

    if (ImGui::BeginTabBar("AnimationGraphInspectorTabs"))
    {
        const ImGuiTabItemFlags StateFlags = bFocusStateDetails ? ImGuiTabItemFlags_SetSelected : 0;

        if (ImGui::BeginTabItem("State", nullptr, StateFlags))
        {
            bFocusStateDetails = false;

            ImGui::BeginChild("StateInspectorScroll");

            DrawStateEditor();

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Parameters"))
        {
            ImGui::BeginChild("ParameterInspectorScroll");

            DrawParameterEditor();

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        const ImGuiTabItemFlags TransitionFlags = bFocusTransitionDetails ? ImGuiTabItemFlags_SetSelected : 0;

        if (ImGui::BeginTabItem("Transitions", nullptr, TransitionFlags))
        {
            bFocusTransitionDetails = false;

            ImGui::BeginChild("TransitionInspectorScroll");

            DrawTransitionEditor();

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Debug"))
        {
            ImGui::BeginChild("RuntimeDebugScroll");

            DrawRuntimeDebug();

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    if (!ErrorMessage.empty())
    {
        ImGui::Separator();
        ImGui::TextUnformatted(ErrorMessage.c_str());
    }
}


void FAnimationGraphEditorTab::DrawGraphCanvas()
{
    const TArray<FAnimGraphStateDefinition>& States = Graph->GetStates();
    const TArray<FAnimGraphTransitionDefinition>& Transitions = Graph->GetTransitions();

    ImGui::TextUnformatted("State Machine");
    ImGui::SameLine();
    ImGui::TextDisabled("%d States / %d Transitions", States.Num(), Transitions.Num());

    if (!NodeEditorContext)
    {
        ImGui::TextDisabled("Node Editor is not initialized.");
        return;
    }

    if (States.IsEmpty())
    {
        ImGui::TextDisabled("No states. Add a State from the Inspector.");
        return;
    }

    ed::EditorContext* PreviousEditor = ed::GetCurrentEditor();
    ed::SetCurrentEditor(NodeEditorContext);

    ed::Style& NodeEditorStyle = ed::GetStyle();
    NodeEditorStyle.Colors[ed::StyleColor_Bg] = ImVec4(0.075f, 0.085f, 0.10f, 1.0f);
    NodeEditorStyle.Colors[ed::StyleColor_Grid] = ImVec4(0.35f, 0.38f, 0.43f, 0.18f);
    NodeEditorStyle.Colors[ed::StyleColor_HovNodeBorder] = ImVec4(0.25f, 0.65f, 0.95f, 1.0f);
    NodeEditorStyle.Colors[ed::StyleColor_SelNodeBorder] = ImVec4(0.95f, 0.68f, 0.25f, 1.0f);
    NodeEditorStyle.Colors[ed::StyleColor_PinRect] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    NodeEditorStyle.Colors[ed::StyleColor_PinRectBorder] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    NodeEditorStyle.PinRadius = 0.0f;
    NodeEditorStyle.PinBorderWidth = 0.0f;
    NodeEditorStyle.NodeBorderWidth = 1.0f;
    NodeEditorStyle.NodeRounding = 7.0f;

    const bool bAutoArrangeRequested = ImGui::Button("Auto Arrange");
    ImGui::SameLine();

    const bool bFrameAllRequested = ImGui::Button("Frame All");
    ImGui::SameLine();

    ImGui::TextDisabled("Zoom %.0f%%", ed::GetCurrentZoom() * 100.0f);

    const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();

    const float InvalidPosition = std::numeric_limits<float>::max();

    const ed::NodeId AnyStateNodeId = MakeAnyStateNodeId();

    const ImVec2 AnyStateDefaultPosition(30.0f, 60.0f);

    auto GetDefaultStatePosition = [](int32 Index)
        {
            return ImVec2(300.0f + static_cast<float>(Index % 3) * 360.0f, 90.0f + static_cast<float>(Index / 3) * 230.0f);
        };

    auto EnsureNodePosition = [&](ed::NodeId NodeId, const FName& StoredName, const ImVec2& DefaultPosition)
        {
            const ImVec2 CurrentPosition = ed::GetNodePosition(NodeId);

            if (CurrentPosition.x != InvalidPosition && CurrentPosition.y != InvalidPosition)
            {
                return;
            }

            float SavedX = 0.0f;
            float SavedY = 0.0f;

            if (Graph->GetEditorNodePosition(StoredName, SavedX, SavedY))
            {
                ed::SetNodePosition(NodeId, ImVec2(SavedX, SavedY));
            }
            else
            {
                ed::SetNodePosition(NodeId, DefaultPosition);
            }
        };

    EnsureNodePosition(AnyStateNodeId, NAME_None, AnyStateDefaultPosition);

    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        EnsureNodePosition(MakeStateNodeId(States[Index].Name), States[Index].Name, GetDefaultStatePosition(Index));
    }

    const FName EntryNodePositionName("__EditorEntryPoint__");
    const FName EntryStateName = Graph->GetEntryState();

    bool bHasEntryState = false;
    ImVec2 EntryDefaultPosition(30.0f, 100.0f);

    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        if (States[Index].Name != EntryStateName)
        {
            continue;
        }

        bHasEntryState = true;

        ImVec2 EntryTargetPosition = GetDefaultStatePosition(Index);

        if (Graph->GetEditorNodePosition(States[Index].Name, EntryTargetPosition.x, EntryTargetPosition.y))
        {
            EntryDefaultPosition = ImVec2(std::max(20.0f, EntryTargetPosition.x - 140.0f), EntryTargetPosition.y + 57.0f);
        }
        else
        {
            EntryDefaultPosition = ImVec2(std::max(20.0f, EntryTargetPosition.x - 140.0f), EntryTargetPosition.y + 57.0f);
        }

        break;
    }

    if (bHasEntryState)
    {
        EnsureNodePosition(MakeEntryNodeId(), EntryNodePositionName, EntryDefaultPosition);
    }

    FAnimStateMachineDebugInfo DebugInfo;

    UAnimInstance* AnimInstance = PreviewActor ? PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance() : nullptr;
    const bool bHasDebugInfo = AnimInstance && AnimInstance->GetStateMachineDebugInfo(DebugInfo);

    bool bRefreshPreview = false;

    auto SubmitEdgePin = [](ed::PinId PinId, ed::PinKind PinKindValue, const ImVec2& NodeMin, const ImVec2& NodeMax, int32 Side)
        {
            constexpr float PinWidth = 28.0f;
            constexpr float PinHeight = 28.0f;

            const float Offset = PinKindValue == ed::PinKind::Input ? -18.0f : 18.0f;

            const float CenterX = (NodeMin.x + NodeMax.x) * 0.5f;
            const float CenterY = (NodeMin.y + NodeMax.y) * 0.5f;

            ImVec2 PinPosition;
            ImVec2 PivotAlignment;
            ImVec2 PinDirection;

            switch (Side)
            {
            case GraphPinSideLeft:
                PinPosition = ImVec2(NodeMin.x, CenterY + Offset - PinHeight * 0.5f);
                PivotAlignment = ImVec2(0.0f, 0.5f);
                PinDirection = ImVec2(-1.0f, 0.0f);
                break;

            case GraphPinSideRight:
                PinPosition = ImVec2(NodeMax.x - PinWidth, CenterY + Offset - PinHeight * 0.5f);
                PivotAlignment = ImVec2(1.0f, 0.5f);
                PinDirection = ImVec2(1.0f, 0.0f);
                break;

            case GraphPinSideTop:
                PinPosition = ImVec2(CenterX + Offset - PinWidth * 0.5f, NodeMin.y);
                PivotAlignment = ImVec2(0.5f, 0.0f);
                PinDirection = ImVec2(0.0f, -1.0f);
                break;

            case GraphPinSideBottom:
                PinPosition = ImVec2(CenterX + Offset - PinWidth * 0.5f, NodeMax.y - PinHeight);
                PivotAlignment = ImVec2(0.5f, 1.0f);
                PinDirection = ImVec2(0.0f, 1.0f);
                break;

            default:
                return;
            }

            ImGui::SetCursorScreenPos(PinPosition);

            ed::PushStyleVar(ed::StyleVar_SourceDirection, PinDirection);
            ed::PushStyleVar(ed::StyleVar_TargetDirection, PinDirection);

            ed::BeginPin(PinId, PinKindValue);
            ed::PinPivotAlignment(PivotAlignment);
            ed::PinPivotSize(ImVec2(0.0f, 0.0f));
            ImGui::Dummy(ImVec2(PinWidth, PinHeight));
            ed::EndPin();

            ed::PopStyleVar(2);
        };

    auto SubmitTransitionPin = [](ed::PinId PinId, ed::PinKind PinKindValue, const ImVec2& NodeMin, const ImVec2& NodeMax, int32 Side, float TangentOffset)
        {
            constexpr float PinSize = 4.0f;

            const float CenterX = (NodeMin.x + NodeMax.x) * 0.5f;
            const float CenterY = (NodeMin.y + NodeMax.y) * 0.5f;

            ImVec2 PinPosition;
            ImVec2 PivotAlignment;
            ImVec2 PinDirection;

            switch (Side)
            {
            case GraphPinSideLeft:
                PinPosition = ImVec2(NodeMin.x, CenterY + TangentOffset - PinSize * 0.5f);
                PivotAlignment = ImVec2(0.0f, 0.5f);
                PinDirection = ImVec2(-1.0f, 0.0f);
                break;

            case GraphPinSideRight:
                PinPosition = ImVec2(NodeMax.x - PinSize, CenterY + TangentOffset - PinSize * 0.5f);
                PivotAlignment = ImVec2(1.0f, 0.5f);
                PinDirection = ImVec2(1.0f, 0.0f);
                break;

            case GraphPinSideTop:
                PinPosition = ImVec2(CenterX + TangentOffset - PinSize * 0.5f, NodeMin.y);
                PivotAlignment = ImVec2(0.5f, 0.0f);
                PinDirection = ImVec2(0.0f, -1.0f);
                break;

            case GraphPinSideBottom:
                PinPosition = ImVec2(CenterX + TangentOffset - PinSize * 0.5f, NodeMax.y - PinSize);
                PivotAlignment = ImVec2(0.5f, 1.0f);
                PinDirection = ImVec2(0.0f, 1.0f);
                break;

            default:
                return;
            }

            ImGui::SetCursorScreenPos(PinPosition);
            ed::PushStyleVar(ed::StyleVar_SourceDirection, PinDirection);
            ed::PushStyleVar(ed::StyleVar_TargetDirection, PinDirection);
            ed::BeginPin(PinId, PinKindValue);
            ed::PinPivotAlignment(PivotAlignment);
            ed::PinPivotSize(ImVec2(0.0f, 0.0f));
            ImGui::Dummy(ImVec2(PinSize, PinSize));
            ed::EndPin();
            ed::PopStyleVar(2);
        };

    constexpr float StateNodeWidth = 240.0f;
    constexpr float StateNodeHeight = 150.0f;
    constexpr float AnyStateNodeWidth = 180.0f;
    constexpr float AnyStateNodeHeight = 58.0f;
    constexpr float EntryNodeWidth = 104.0f;
    constexpr float EntryNodeHeight = 36.0f;

    auto GetKnownNodeSize = [&](ed::NodeId NodeId)
        {
            if (NodeId == AnyStateNodeId)
            {
                return ImVec2(AnyStateNodeWidth, AnyStateNodeHeight);
            }

            if (NodeId == MakeEntryNodeId())
            {
                return ImVec2(EntryNodeWidth, EntryNodeHeight);
            }

            return ImVec2(StateNodeWidth, StateNodeHeight);
        };

    auto GetNodeCenter = [&](ed::NodeId NodeId)
        {
            const ImVec2 Position = ed::GetNodePosition(NodeId);
            const ImVec2 Size = GetKnownNodeSize(NodeId);
            return ImVec2(Position.x + Size.x * 0.5f, Position.y + Size.y * 0.5f);
        };

    auto GetFacingPinSides = [&](ed::NodeId SourceNodeId, ed::NodeId DestinationNodeId, int32& OutSourceSide, int32& OutDestinationSide)
        {
            OutSourceSide = GraphPinSideRight;
            OutDestinationSide = GraphPinSideLeft;

            const ImVec2 SourcePosition = ed::GetNodePosition(SourceNodeId);
            const ImVec2 DestinationPosition = ed::GetNodePosition(DestinationNodeId);

            if (SourcePosition.x == InvalidPosition || DestinationPosition.x == InvalidPosition)
            {
                return;
            }

            const ImVec2 SourceCenter = GetNodeCenter(SourceNodeId);
            const ImVec2 DestinationCenter = GetNodeCenter(DestinationNodeId);

            const float DeltaX = DestinationCenter.x - SourceCenter.x;
            const float DeltaY = DestinationCenter.y - SourceCenter.y;

            if (std::abs(DeltaX) >= std::abs(DeltaY))
            {
                OutSourceSide = DeltaX >= 0.0f ? GraphPinSideRight : GraphPinSideLeft;
                OutDestinationSide = DeltaX >= 0.0f ? GraphPinSideLeft : GraphPinSideRight;
            }
            else
            {
                OutSourceSide = DeltaY >= 0.0f ? GraphPinSideBottom : GraphPinSideTop;
                OutDestinationSide = DeltaY >= 0.0f ? GraphPinSideTop : GraphPinSideBottom;
            }
        };

    struct FTransitionRoute
    {
        FAnimGraphTransitionDefinition Transition;
        ed::NodeId SourceNodeId;
        ed::NodeId DestinationNodeId;
        int32 SourceSide = GraphPinSideRight;
        int32 DestinationSide = GraphPinSideLeft;
        float SourceOffset = 0.0f;
        float DestinationOffset = 0.0f;
    };

    std::vector<FTransitionRoute> TransitionRoutes;
    TransitionRoutes.reserve(Transitions.Num());

    for (int32 Index = 0; Index < Transitions.Num(); ++Index)
    {
        FTransitionRoute Route;
        Route.Transition = Transitions[Index];
        Route.SourceNodeId = Route.Transition.bAnyState ? AnyStateNodeId : MakeStateNodeId(Route.Transition.FromState);
        Route.DestinationNodeId = MakeStateNodeId(Route.Transition.ToState);

        GetFacingPinSides(Route.SourceNodeId, Route.DestinationNodeId, Route.SourceSide, Route.DestinationSide);
        TransitionRoutes.push_back(Route);
    }

    auto GetPeerTangentCoordinate = [&](ed::NodeId PeerNodeId, int32 Side)
        {
            const ImVec2 Center = GetNodeCenter(PeerNodeId);
            return Side == GraphPinSideLeft || Side == GraphPinSideRight ? Center.y : Center.x;
        };

    auto CalculateTransitionLaneOffset = [&](int32 RouteIndex, bool bSource)
        {
            const FTransitionRoute& CurrentRoute = TransitionRoutes[RouteIndex];

            const ed::NodeId OwnerNodeId = bSource ? CurrentRoute.SourceNodeId : CurrentRoute.DestinationNodeId;
            const ed::NodeId PeerNodeId = bSource ? CurrentRoute.DestinationNodeId : CurrentRoute.SourceNodeId;
            const int32 Side = bSource ? CurrentRoute.SourceSide : CurrentRoute.DestinationSide;

            const float PeerCoordinate = GetPeerTangentCoordinate(PeerNodeId, Side);

            int32 Count = 0;
            int32 Rank = 0;

            for (int32 OtherIndex = 0; OtherIndex < static_cast<int32>(TransitionRoutes.size()); ++OtherIndex)
            {
                const FTransitionRoute& OtherRoute = TransitionRoutes[OtherIndex];

                for (int32 EndpointIndex = 0; EndpointIndex < 2; ++EndpointIndex)
                {
                    const bool bOtherSource = EndpointIndex == 0;

                    const ed::NodeId OtherOwnerNodeId = bOtherSource ? OtherRoute.SourceNodeId : OtherRoute.DestinationNodeId;
                    const ed::NodeId OtherPeerNodeId = bOtherSource ? OtherRoute.DestinationNodeId : OtherRoute.SourceNodeId;
                    const int32 OtherSide = bOtherSource ? OtherRoute.SourceSide : OtherRoute.DestinationSide;

                    if (OtherOwnerNodeId != OwnerNodeId || OtherSide != Side)
                    {
                        continue;
                    }

                    const float OtherCoordinate = GetPeerTangentCoordinate(OtherPeerNodeId, Side);

                    ++Count;

                    const bool bSameCoordinate = std::abs(OtherCoordinate - PeerCoordinate) <= 0.01f;
                    const bool bComesBefore = OtherCoordinate < PeerCoordinate - 0.01f ||
                        (bSameCoordinate && (OtherIndex < RouteIndex || (OtherIndex == RouteIndex && bOtherSource && !bSource)));

                    if (bComesBefore)
                    {
                        ++Rank;
                    }
                }
            }

            const ImVec2 NodeSize = GetKnownNodeSize(OwnerNodeId);
            const float EdgeLength = Side == GraphPinSideLeft || Side == GraphPinSideRight ? NodeSize.y : NodeSize.x;
            const float MaxRange = std::max(0.0f, EdgeLength * 0.5f - 18.0f);
            const float Spacing = Count > 1 ? std::min(22.0f, MaxRange * 2.0f / static_cast<float>(Count - 1)) : 0.0f;

            return (static_cast<float>(Rank) - static_cast<float>(Count - 1) * 0.5f) * Spacing;
        };

    for (int32 Index = 0; Index < static_cast<int32>(TransitionRoutes.size()); ++Index)
    {
        FTransitionRoute& Route = TransitionRoutes[Index];
        Route.SourceOffset = CalculateTransitionLaneOffset(Index, true);
        Route.DestinationOffset = CalculateTransitionLaneOffset(Index, false);
    }

    ed::Begin("AnimationGraphNodeEditor", CanvasSize);

    if (bAutoArrangeRequested)
    {
        ed::SetNodePosition(AnyStateNodeId, AnyStateDefaultPosition);
        Graph->SetEditorNodePosition(NAME_None, AnyStateDefaultPosition.x, AnyStateDefaultPosition.y);

        for (int32 Index = 0; Index < States.Num(); ++Index)
        {
            const ImVec2 Position = GetDefaultStatePosition(Index);
            ed::SetNodePosition(MakeStateNodeId(States[Index].Name), Position);
            Graph->SetEditorNodePosition(States[Index].Name, Position.x, Position.y);
        }

        bDirty = true;
    }

    // Any State Node
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.12f, 0.16f, 0.14f, 1.0f));
    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.27f, 0.42f, 0.34f, 1.0f));
    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);

    ed::BeginNode(AnyStateNodeId);

    constexpr float AnyStateWidth = 180.0f;
    constexpr float AnyStateHeight = 58.0f;

    const ImVec2 AnyStateMin = ImGui::GetCursorScreenPos();
    const ImVec2 AnyStateMax(AnyStateMin.x + AnyStateWidth, AnyStateMin.y + AnyStateHeight);
    ImDrawList* AnyStateDrawList = ImGui::GetWindowDrawList();

    AnyStateDrawList->AddRectFilled(AnyStateMin, AnyStateMax, IM_COL32(35, 48, 40, 255), 7.0f);
    AnyStateDrawList->AddRectFilled(AnyStateMin, ImVec2(AnyStateMax.x, AnyStateMin.y + 34.0f), IM_COL32(53, 112, 77, 255), 7.0f, ImDrawFlags_RoundCornersTop);
    AnyStateDrawList->AddRect(AnyStateMin, AnyStateMax, IM_COL32(90, 135, 103, 255), 7.0f);

    AnyStateDrawList->AddText(ImVec2(AnyStateMin.x + 12.0f, AnyStateMin.y + 10.0f), IM_COL32(245, 245, 245, 255), "Any State");
    AnyStateDrawList->AddText(ImVec2(AnyStateMin.x + 12.0f, AnyStateMin.y + 40.0f), IM_COL32(165, 195, 175, 255), "Global Transition");

    ImGui::SetCursorScreenPos(ImVec2(AnyStateMax.x - 2.0f, AnyStateMin.y + AnyStateHeight * 0.5f - 1.0f));

    for (int32 Side = 0; Side < GraphPinSideCount; ++Side)
    {
        SubmitEdgePin(MakeAnyStateOutputPinId(Side), ed::PinKind::Output, AnyStateMin, AnyStateMax, Side);
    }

    for (const FTransitionRoute& Route : TransitionRoutes)
    {
        if (Route.SourceNodeId == AnyStateNodeId)
        {
            SubmitTransitionPin(MakeTransitionSourcePinId(Route.Transition), ed::PinKind::Output, AnyStateMin, AnyStateMax, Route.SourceSide, Route.SourceOffset);
        }
    }

    ImGui::SetCursorScreenPos(AnyStateMin);
    ImGui::Dummy(ImVec2(AnyStateWidth, AnyStateHeight));

    ed::EndNode();

    ed::PopStyleVar(2);
    ed::PopStyleColor(2);

    // Entry Node
    if (bHasEntryState)
    {
        ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.16f, 0.17f, 0.19f, 1.0f));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.55f, 0.58f, 0.63f, 1.0f));
        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ed::BeginNode(MakeEntryNodeId());

        const ImVec2 EntryMin = ImGui::GetCursorScreenPos();
        const ImVec2 EntryMax(EntryMin.x + 104.0f, EntryMin.y + 36.0f);
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        DrawList->AddRectFilled(EntryMin, EntryMax, IM_COL32(55, 59, 66, 255), 6.0f);
        DrawList->AddRect(EntryMin, EntryMax, IM_COL32(135, 142, 153, 255), 6.0f);
        DrawList->AddText(ImVec2(EntryMin.x + 10.0f, EntryMin.y + 10.0f), IM_COL32(245, 245, 245, 255), "Entry");

        DrawList->AddTriangleFilled(ImVec2(EntryMin.x + 80.0f, EntryMin.y + 11.0f), ImVec2(EntryMin.x + 80.0f, EntryMin.y + 25.0f), ImVec2(EntryMin.x + 91.0f, EntryMin.y + 18.0f), IM_COL32(230, 230, 230, 255));

        ImGui::SetCursorScreenPos(ImVec2(EntryMax.x - 2.0f, EntryMin.y + 17.0f));
        
        for (int32 Side = 0; Side < GraphPinSideCount; ++Side)
        {
            SubmitEdgePin(MakeEntryOutputPinId(Side), ed::PinKind::Output, EntryMin, EntryMax, Side);
        }

        ImGui::SetCursorScreenPos(EntryMin);
        ImGui::Dummy(ImVec2(104.0f, 36.0f));

        ed::EndNode();

        ed::PopStyleVar();
        ed::PopStyleColor(2);
    }

    // State Nodes
    constexpr float NodeWidth = 240.0f;
    constexpr float NodeHeight = 150.0f;
    constexpr float HeaderHeight = 34.0f;

    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        const FAnimGraphStateDefinition& State = States[Index];

        const bool bCurrent = bHasDebugInfo && DebugInfo.CurrentStateName == State.Name;

        const FString StateName = State.Name.ToString();
        const char* NodeType = State.NodeType == EAnimGraphStateNodeType::SequencePlayer ? "Sequence Player" : "BlendSpace Player";

        const FString AssetPath = State.NodeType == EAnimGraphStateNodeType::SequencePlayer
            ? (State.Sequence ? State.Sequence->GetPath() : "Unassigned Asset")
            : (State.BlendSpace ? State.BlendSpace->GetPath() : "Unassigned Asset");

        const size_t AssetNameStart = AssetPath.find_last_of("/\\");
        const FString AssetName = AssetNameStart == FString::npos ? AssetPath : AssetPath.substr(AssetNameStart + 1);

        const ImU32 HeaderColor = bCurrent ? IM_COL32(48, 132, 91, 255)
            : State.NodeType == EAnimGraphStateNodeType::SequencePlayer ? IM_COL32(48, 101, 139, 255)
            : IM_COL32(105, 76, 143, 255);

        ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.28f, 0.30f, 0.34f, 1.0f));
        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ed::PushStyleVar(ed::StyleVar_NodeRounding, 7.0f);

        ed::BeginNode(MakeStateNodeId(State.Name));

        const ImVec2 NodeMin = ImGui::GetCursorScreenPos();
        const ImVec2 NodeMax(NodeMin.x + NodeWidth, NodeMin.y + NodeHeight);

        ImDrawList* NodeDrawList = ImGui::GetWindowDrawList();

        // 노드 배경
        NodeDrawList->AddRectFilled(NodeMin, NodeMax, IM_COL32(30, 33, 39, 255), 7.0f);
        NodeDrawList->AddRectFilled(NodeMin, ImVec2(NodeMax.x, NodeMin.y + HeaderHeight), HeaderColor, 7.0f, ImDrawFlags_RoundCornersTop);
        NodeDrawList->AddRect(NodeMin, NodeMax, IM_COL32(75, 80, 91, 255), 7.0f);

        // 헤더 이름
        const ImVec4 HeaderClip(NodeMin.x + 10.0f, NodeMin.y + 3.0f, NodeMax.x - 10.0f, NodeMin.y + HeaderHeight - 3.0f);

        NodeDrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(NodeMin.x + 10.0f, NodeMin.y + 8.0f), IM_COL32(245, 245, 250, 255), StateName.c_str(), nullptr, 0.0f, &HeaderClip);

        // 노드 본문
        const ImVec4 BodyClip(NodeMin.x + 10.0f, NodeMin.y + 39.0f, NodeMax.x - 10.0f, NodeMax.y - 8.0f);

        NodeDrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(NodeMin.x + 12.0f, NodeMin.y + 43.0f), IM_COL32(170, 185, 205, 255), NodeType, nullptr, 0.0f, &BodyClip);
        NodeDrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(NodeMin.x + 12.0f, NodeMin.y + 68.0f), IM_COL32(135, 145, 160, 255), "ASSET", nullptr, 0.0f, &BodyClip);
        NodeDrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(NodeMin.x + 12.0f, NodeMin.y + 87.0f), IM_COL32(230, 232, 237, 255), AssetName.c_str(), nullptr, 0.0f, &BodyClip);

        char PlayRateText[64];
        std::snprintf(PlayRateText, sizeof(PlayRateText), "Play Rate: %.2fx  |  %s", State.PlayRate, State.bLooping ? "Loop" : "Once");

        NodeDrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(NodeMin.x + 12.0f, NodeMin.y + 113.0f), IM_COL32(175, 185, 198, 255), PlayRateText, nullptr, 0.0f, &BodyClip);

        if (bCurrent)
        {
            NodeDrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(NodeMin.x + 12.0f, NodeMin.y + 132.0f), IM_COL32(90, 225, 145, 255), "ACTIVE", nullptr, 0.0f, &BodyClip);
        }

        for (int32 Side = 0; Side < GraphPinSideCount; ++Side)
        {
            SubmitEdgePin(MakeStateInputPinId(State.Name, Side), ed::PinKind::Input, NodeMin, NodeMax, Side);
            SubmitEdgePin(MakeStateOutputPinId(State.Name, Side), ed::PinKind::Output, NodeMin, NodeMax, Side);
        }

        const ed::NodeId CurrentStateNodeId = MakeStateNodeId(State.Name);

        for (const FTransitionRoute& Route : TransitionRoutes)
        {
            if (Route.SourceNodeId == CurrentStateNodeId)
            {
                SubmitTransitionPin(MakeTransitionSourcePinId(Route.Transition), ed::PinKind::Output, NodeMin, NodeMax, Route.SourceSide, Route.SourceOffset);
            }

            if (Route.DestinationNodeId == CurrentStateNodeId)
            {
                SubmitTransitionPin(MakeTransitionDestinationPinId(Route.Transition), ed::PinKind::Input, NodeMin, NodeMax, Route.DestinationSide, Route.DestinationOffset);
            }
        }

        ImGui::SetCursorScreenPos(NodeMin);
        ImGui::Dummy(ImVec2(NodeWidth, NodeHeight));

        ed::EndNode();

        ed::PopStyleVar(2);
        ed::PopStyleColor(2);
    }

    // Entry -> Default State
    if (bHasEntryState)
    {
        int32 EntrySourceSide = GraphPinSideRight;
        int32 EntryDestinationSide = GraphPinSideLeft;

        GetFacingPinSides(MakeEntryNodeId(), MakeStateNodeId(EntryStateName), EntrySourceSide, EntryDestinationSide);

        ed::Link(MakeEntryLinkId(), MakeEntryOutputPinId(EntrySourceSide), MakeStateInputPinId(EntryStateName, EntryDestinationSide), ImVec4(0.88f, 0.88f, 0.88f, 1.0f), 2.0f);
    }


    for (int32 Index = 0; Index < static_cast<int32>(TransitionRoutes.size()); ++Index)
    {
        const FTransitionRoute& Route = TransitionRoutes[Index];
        const FAnimGraphTransitionDefinition& Transition = Route.Transition;

        const ed::PinId SourcePin = MakeTransitionSourcePinId(Transition);
        const ed::PinId DestinationPin = MakeTransitionDestinationPinId(Transition);

        const bool bSelected = SelectedTransitionIndex == Index;

        const bool bLastTransition = bHasDebugInfo && DebugInfo.bHasLastTransition && DebugInfo.LastTransitionToState == Transition.ToState &&
            DebugInfo.LastTransitionFromState == Transition.FromState && DebugInfo.bLastTransitionWasAnyState == Transition.bAnyState;

        const ImVec4 LinkColor = bSelected ? ImVec4(1.0f, 0.78f, 0.35f, 1.0f) : bLastTransition ? ImVec4(0.35f, 0.86f, 0.59f, 1.0f) : ImVec4(0.61f, 0.65f, 0.71f, 1.0f);

        ed::Link(MakeTransitionLinkId(Transition), SourcePin, DestinationPin, LinkColor, bSelected || bLastTransition ? 3.0f : 2.0f);
    }

    if (ed::BeginCreate())
    {
        ed::PinId StartPinId;
        ed::PinId EndPinId;

        if (ed::QueryNewLink(&StartPinId, &EndPinId))
        {
            auto ResolveOutputPin = [&](ed::PinId PinId, FName& OutSourceState, bool& bOutAnyState)
                {
                    if (PinId == MakeAnyStateOutputPinId(GraphPinSideLeft) ||
                        PinId == MakeAnyStateOutputPinId(GraphPinSideRight) ||
                        PinId == MakeAnyStateOutputPinId(GraphPinSideTop) ||
                        PinId == MakeAnyStateOutputPinId(GraphPinSideBottom))
                    {
                        OutSourceState = NAME_None;
                        bOutAnyState = true;
                        return true;
                    }

                    for (const FTransitionRoute& Route : TransitionRoutes)
                    {
                        if (PinId == MakeTransitionSourcePinId(Route.Transition))
                        {
                            OutSourceState = Route.Transition.FromState;
                            bOutAnyState = Route.Transition.bAnyState;
                            return true;
                        }
                    }

                    for (const FAnimGraphStateDefinition& State : States)
                    {
                        for (int32 Side = 0; Side < GraphPinSideCount; ++Side)
                        {
                            if (PinId == MakeStateOutputPinId(State.Name, Side))
                            {
                                OutSourceState = State.Name;
                                bOutAnyState = false;
                                return true;
                            }
                        }
                    }

                    return false;
                };

            auto ResolveInputPin = [&](ed::PinId PinId, FName& OutDestinationState)
                {
                    for (const FTransitionRoute& Route : TransitionRoutes)
                    {
                        if (PinId == MakeTransitionDestinationPinId(Route.Transition))
                        {
                            OutDestinationState = Route.Transition.ToState;
                            return true;
                        }
                    }

                    for (const FAnimGraphStateDefinition& State : States)
                    {
                        for (int32 Side = 0; Side < GraphPinSideCount; ++Side)
                        {
                            if (PinId == MakeStateInputPinId(State.Name, Side))
                            {
                                OutDestinationState = State.Name;
                                return true;
                            }
                        }
                    }

                    return false;
                };

            FName FromState = NAME_None;
            FName ToState = NAME_None;
            bool bAnyState = false;

            const bool bValidPins = (ResolveOutputPin(StartPinId, FromState, bAnyState) && ResolveInputPin(EndPinId, ToState)) ||
                (ResolveOutputPin(EndPinId, FromState, bAnyState) && ResolveInputPin(StartPinId, ToState));

            if (!bValidPins)
            {
                ed::RejectNewItem();
            }
            else if (!bAnyState && FromState == ToState)
            {
                ed::RejectNewItem();
                ErrorMessage = "A State cannot transition to itself.";
            }
            else
            {
                int32 ExistingTransitionIndex = -1;

                const TArray<FAnimGraphTransitionDefinition>& CurrentTransitions = Graph->GetTransitions();

                for (int32 Index = 0; Index < CurrentTransitions.Num(); ++Index)
                {
                    const FAnimGraphTransitionDefinition& Existing = CurrentTransitions[Index];

                    if (Existing.bAnyState == bAnyState && Existing.FromState == FromState && Existing.ToState == ToState)
                    {
                        ExistingTransitionIndex = Index;
                        break;
                    }
                }

                if (ExistingTransitionIndex != -1)
                {
                    ed::RejectNewItem();

                    SelectedTransitionIndex = ExistingTransitionIndex;
                    SelectedStateIndex = -1;
                    bFocusTransitionDetails = true;
                    bFocusStateDetails = false;
                    ErrorMessage.clear();
                }
                else if (ed::AcceptNewItem())
                {
                    const bool bAdded = bAnyState
                        ? Graph->AddAnyStateTransition(ToState, EAnimTransitionRule::StateFinished, NAME_None, 0, 0.2f, 0.0f)
                        : Graph->AddTransition(FromState, ToState, EAnimTransitionRule::StateFinished, NAME_None, 0, 0.2f, 0.0f);

                    if (bAdded)
                    {
                        SelectedTransitionIndex = Graph->GetTransitions().Num() - 1;
                        SelectedStateIndex = -1;
                        bFocusTransitionDetails = true;
                        bFocusStateDetails = false;

                        bDirty = true;
                        bRefreshPreview = true;
                        ErrorMessage.clear();
                    }
                    else
                    {
                        ErrorMessage = "Could not create Transition.";
                    }
                }
            }
        }
    }

    ed::EndCreate();
    if (ed::BeginDelete())
    {
        ed::NodeId DeletedNodeId;

        while (ed::QueryDeletedNode(&DeletedNodeId))
        {
            ed::RejectDeletedItem();
        }

        ed::LinkId DeletedLinkId;

        while (ed::QueryDeletedLink(&DeletedLinkId))
        {
            int32 TransitionIndex = -1;

            const TArray<FAnimGraphTransitionDefinition>& CurrentTransitions = Graph->GetTransitions();

            for (int32 Index = 0; Index < CurrentTransitions.Num(); ++Index)
            {
                if (MakeTransitionLinkId(CurrentTransitions[Index]) == DeletedLinkId)
                {
                    TransitionIndex = Index;
                    break;
                }
            }

            if (TransitionIndex == -1)
            {
                ed::RejectDeletedItem();
                continue;
            }

            if (ed::AcceptDeletedItem())
            {
                if (Graph->RemoveTransition(TransitionIndex))
                {
                    if (SelectedTransitionIndex == TransitionIndex)
                    {
                        SelectedTransitionIndex = -1;
                        bFocusTransitionDetails = false;
                    }
                    else if (SelectedTransitionIndex > TransitionIndex)
                    {
                        --SelectedTransitionIndex;
                    }

                    bDirty = true;
                    bRefreshPreview = true;
                    ErrorMessage.clear();
                }
            }
        }

        ed::EndDelete();
    }

    ed::End();

    if (bAutoArrangeRequested || bFrameAllRequested)
    {
        ed::NavigateToContent(0.25f);
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    for (const FAnimGraphTransitionDefinition& Transition : Graph->GetTransitions())
    {
        const ed::NodeId SourceNodeId = Transition.bAnyState ? AnyStateNodeId : MakeStateNodeId(Transition.FromState);
        const ed::NodeId DestinationNodeId = MakeStateNodeId(Transition.ToState);

        const ImVec2 SourcePosition = ed::GetNodePosition(SourceNodeId);
        const ImVec2 DestinationPosition = ed::GetNodePosition(DestinationNodeId);

        if (SourcePosition.x == InvalidPosition || DestinationPosition.x == InvalidPosition)
        {
            continue;
        }

        const ImVec2 SourceSize = ed::GetNodeSize(SourceNodeId);
        const ImVec2 DestinationSize = ed::GetNodeSize(DestinationNodeId);

        const ImVec2 SourceCenter(SourcePosition.x + SourceSize.x * 0.5f, SourcePosition.y + SourceSize.y * 0.5f);
        const ImVec2 DestinationCenter(DestinationPosition.x + DestinationSize.x * 0.5f, DestinationPosition.y + DestinationSize.y * 0.5f);

        const float DeltaX = DestinationCenter.x - SourceCenter.x;
        const float DeltaY = DestinationCenter.y - SourceCenter.y;
        const float Length = std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY);

        const float DirectionSign = SourceNodeId.Get() < DestinationNodeId.Get() ? 1.0f : -1.0f;

        const ImVec2 Perpendicular = Length > 0.001f
            ? ImVec2(-DeltaY / Length * DirectionSign, DeltaX / Length * DirectionSign)
            : ImVec2(0.0f, 1.0f);

        float RouteOffset = 0.0f;

        for (const FTransitionRoute& Route : TransitionRoutes)
        {
            if (MakeTransitionLinkId(Route.Transition) == MakeTransitionLinkId(Transition))
            {
                RouteOffset = (Route.SourceOffset + Route.DestinationOffset) * 0.5f;
                break;
            }
        }

        for (const FTransitionRoute& Route : TransitionRoutes)
        {
            if (MakeTransitionLinkId(Route.Transition) == MakeTransitionLinkId(Transition))
            {
                RouteOffset = (Route.SourceOffset + Route.DestinationOffset) * 0.25f;
                break;
            }
        }

        const ImVec2 CanvasCenter(
            (SourceCenter.x + DestinationCenter.x) * 0.5f + Perpendicular.x * RouteOffset,
            (SourceCenter.y + DestinationCenter.y) * 0.5f + Perpendicular.y * RouteOffset);

        const ImVec2 LabelCenter = ed::CanvasToScreen(CanvasCenter);

        const FString RuleLabel = GetTransitionConditionLabel(Transition);
        const ImVec2 LabelSize = ImGui::CalcTextSize(RuleLabel.c_str());

        DrawList->AddRectFilled(ImVec2(LabelCenter.x - LabelSize.x * 0.5f - 5.0f, LabelCenter.y - 10.0f), ImVec2(LabelCenter.x + LabelSize.x * 0.5f + 5.0f, LabelCenter.y + 10.0f), IM_COL32(35, 38, 44, 245), 4.0f);
        DrawList->AddText(ImVec2(LabelCenter.x - LabelSize.x * 0.5f, LabelCenter.y - LabelSize.y * 0.5f), IM_COL32(220, 225, 235, 255), RuleLabel.c_str());
    }

    if (ed::HasSelectionChanged())
    {
        ed::NodeId SelectedNodeIds[1];
        ed::LinkId SelectedLinkIds[1];

        const int32 SelectedNodeCount = ed::GetSelectedNodes(SelectedNodeIds, 1);
        const int32 SelectedLinkCount = ed::GetSelectedLinks(SelectedLinkIds, 1);

        if (SelectedNodeCount > 0)
        {
            SelectedStateIndex = -1;
            SelectedTransitionIndex = -1;

            if (SelectedNodeIds[0] == AnyStateNodeId)
            {
                bFocusStateDetails = false;
                bFocusTransitionDetails = false;
            }
            else
            {
                for (int32 Index = 0; Index < States.Num(); ++Index)
                {
                    if (MakeStateNodeId(States[Index].Name) == SelectedNodeIds[0])
                    {
                        SelectedStateIndex = Index;

                        const FString StateName = States[Index].Name.ToString();
                        std::snprintf(RenameStateBuffer, sizeof(RenameStateBuffer), "%s", StateName.c_str());

                        bFocusStateDetails = true;
                        bFocusTransitionDetails = false;
                        break;
                    }
                }
            }
        }
        else if (SelectedLinkCount > 0)
        {
            SelectedStateIndex = -1;
            SelectedTransitionIndex = -1;

            const TArray<FAnimGraphTransitionDefinition>& CurrentTransitions = Graph->GetTransitions();

            for (int32 Index = 0; Index < CurrentTransitions.Num(); ++Index)
            {
                if (MakeTransitionLinkId(CurrentTransitions[Index]) == SelectedLinkIds[0])
                {
                    SelectedTransitionIndex = Index;
                    bFocusTransitionDetails = true;
                    bFocusStateDetails = false;
                    break;
                }
            }
        }
        else
        {
            SelectedStateIndex = -1;
            SelectedTransitionIndex = -1;
            bFocusStateDetails = false;
            bFocusTransitionDetails = false;
        }
    }

    auto SaveNodePosition = [&](ed::NodeId NodeId, const FName& StoredName, const ImVec2& DefaultPosition)
        {
            const ImVec2 Position = ed::GetNodePosition(NodeId);

            if (Position.x == InvalidPosition || Position.y == InvalidPosition)
            {
                return;
            }

            float SavedX = 0.0f;
            float SavedY = 0.0f;

            if (Graph->GetEditorNodePosition(StoredName, SavedX, SavedY))
            {
                if (std::abs(SavedX - Position.x) > 0.5f || std::abs(SavedY - Position.y) > 0.5f)
                {
                    Graph->SetEditorNodePosition(StoredName, Position.x, Position.y);
                    bDirty = true;
                }
            }
            else if (std::abs(DefaultPosition.x - Position.x) > 0.5f || std::abs(DefaultPosition.y - Position.y) > 0.5f)
            {
                Graph->SetEditorNodePosition(StoredName, Position.x, Position.y);
                bDirty = true;
            }
        };

    SaveNodePosition(AnyStateNodeId, NAME_None, AnyStateDefaultPosition);

    if (bHasEntryState)
    {
        SaveNodePosition(MakeEntryNodeId(), EntryNodePositionName, EntryDefaultPosition);
    }

    for (int32 Index = 0; Index < States.Num(); ++Index)
    {
        SaveNodePosition(MakeStateNodeId(States[Index].Name), States[Index].Name, GetDefaultStatePosition(Index));
    }

    ed::SetCurrentEditor(PreviousEditor);

    if (bRefreshPreview)
    {
        RefreshPreview();
    }
}
