#pragma once

#include "Editor/FEditorTab.h"
#include "PreviewScene.h"
#include "Animation/BlendSpace1D.h"
class UBlendSpace1D;
class UAnimSequence;
class USkeletalMesh;
class ASkeletalMeshActor;
class FPreviewViewportPanel;

class FBlendSpaceEditorTab : public FEditorTab
{
public:
    explicit FBlendSpaceEditorTab(UBlendSpace1D* InBlendSpace);
    virtual ~FBlendSpaceEditorTab() override;
    FBlendSpaceEditorTab(const FBlendSpaceEditorTab&) = delete;
    FBlendSpaceEditorTab& operator=(const FBlendSpaceEditorTab&) = delete;
    void Tick(float DeltaTime) override;
    void RouteInput(const FViewportInput& BaseInput) override;
    void CollectViews(TArray<FEditorTabView>& OutViews) override;
    UWorld* GetWorld() const override;
    const UObject* GetEditingAsset() const override { return BlendSpace; }

private:
    void DrawToolbar();
    void FrameMesh();
    void RefreshPreview(float PreviousTime);
    bool CanCloseNow() override;
    bool SaveBlendSpaceAsset();

    FString ErrorMessage;

    bool bShowClosePopup = false;
    UBlendSpace1D* BlendSpace = nullptr;
    USkeletalMesh* SkeletalMesh = nullptr;

    TUniquePtr<FPreviewScene> PreviewScene;

    ASkeletalMeshActor* PreviewActor = nullptr;
    FPreviewViewportPanel* ViewportPanel = nullptr;

    TArray<UAnimSequence*> AvailableAnimations;

    UAnimSequence* NewSampleAnimation = nullptr;

    float NewSampleValue = 300.0f;
    float NewSampleRateScale = 1.0f;
    float BlendInput = 0.0f;

    bool bFramed = false;
    bool bPlaying = true;
    bool bDirty = false;
};