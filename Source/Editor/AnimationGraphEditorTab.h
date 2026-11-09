#pragma once

#include "Editor/FEditorTab.h"
#include "PreviewScene.h"
#include "Animation/AnimationGraph.h"

class UAnimSequence;
class UBlendSpace1D;
class USkeletalMesh;
class ASkeletalMeshActor;
class FPreviewViewportPanel;

namespace ax
{
    namespace NodeEditor
    {
        struct EditorContext;
    }
}

class FAnimationGraphEditorTab : public FEditorTab
{
public:
    explicit FAnimationGraphEditorTab(UAnimationGraph* InGraph);
    virtual ~FAnimationGraphEditorTab() override;
    FAnimationGraphEditorTab(const FAnimationGraphEditorTab&) = delete;
    FAnimationGraphEditorTab& operator=(const FAnimationGraphEditorTab&) = delete;
    void Tick(float DeltaTime) override;
    void RouteInput(const FViewportInput& BaseInput) override;
    void CollectViews(TArray<FEditorTabView>& OutViews) override;
    UWorld* GetWorld() const override;
    const UObject* GetEditingAsset() const override { return Graph; }
protected:
    void BuildDefaultLayout(ImGuiID DockSpaceRoot) override;
private:
    void DrawToolbar();
    void DrawStateEditor();
    void DrawParameterEditor();
    void DrawTransitionEditor();
    void DrawRuntimeDebug();
    void DrawGraphEditor();
    void DrawGraphCanvas();
    void FrameMesh();
    void RefreshPreview();
    bool CanCloseNow() override;
    bool SaveAnimationGraphAsset();
private:
	UAnimationGraph* Graph = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
	ASkeletalMeshActor* PreviewActor = nullptr;
	TUniquePtr<FPreviewScene> PreviewScene;
	FPreviewViewportPanel* ViewportPanel = nullptr;
    IEditorPanel* GraphEditorPanel = nullptr;
    ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;
    bool bFocusStateDetails = false;
    bool bFocusTransitionDetails = false;
    TArray<UAnimSequence*> AvailableAnimations;
    TArray<UBlendSpace1D*> AvailableBlendSpaces;

    UAnimSequence* NewSequence = nullptr;
    UBlendSpace1D* NewBlendSpace = nullptr;

    int32 SelectedStateIndex = -1;
    int32 DraggingNodeIndex = -1;
    int32 DraggingFromStateIndex = -1;
    bool bDraggingAnyStateNode = false;
    bool bDraggingAnyStateTransition = false;
    bool bIsTransitionDragging = false;
    float NodeDragOffsetX = 0.0f;
    float NodeDragOffsetY = 0.0f;

    char NewStateNameBuffer[64] = "NewState";
    char RenameStateBuffer[64] = {};

    FString ErrorMessage;

    bool bShowClosePopup = false;
    bool bDirty = false;
    bool bPlaying = false;
    bool bFramed = false;

    // Parameter Editor
    int32 SelectedParameterIndex = -1;

    char NewParameterNameBuffer[64] = "Speed";
    char RenameParameterBuffer[64] = {};

    int32 NewParameterTypeIndex = 1;
    bool bNewParameterBoolDefault = false;
    float NewParameterFloatDefault = 0.0f;

    // Transition Editor
    int32 SelectedTransitionIndex = -1;
    int32 EditableTransitionIndex = -1;
    FAnimGraphTransitionDefinition EditableTransition;
    FAnimGraphTransitionDefinition NewTransition;

    // BlendSpace Parameter Binding
    FName NewBlendSpaceParameterName = NAME_None;
};