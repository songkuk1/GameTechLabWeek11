#pragma once

#include "Core/Types.h"
#include "Math/EngineMath.h"
#include "Math/Box.h"
#include "Math/Color.h"

class FScene;
class UDecalComponent;
class UMaterial;

// 씬 소유 투영 캐시
class FDecalSceneProxy
{
public:
    FDecalSceneProxy(UDecalComponent* InComponent, FScene* InScene);
    void Refresh();

    UDecalComponent* GetComponent() const { return Component; }
    FScene* GetScene() const { return Scene; }
    const FMatrix& GetDecalToWorld() const { return DecalToWorld; }
    const FMatrix& GetWorldToDecal() const { return WorldToDecal; }
    const FBox& GetBounds() const { return Bounds; }
    const FVector& GetForward() const { return Forward; }
    UMaterial* GetMaterial() const { return Material; }
    const FLinearColor& GetColor() const { return Color; }
    int32 GetSortOrder() const { return SortOrder; }
    uint32 GetId() const { return Id; }
    float GetAngleFadeStart() const { return AngleFadeStart; }
    float GetAngleFadeEnd() const { return AngleFadeEnd; }
    float GetFadeAlpha() const { return FadeAlpha; }
    bool IsVisible() const;

    bool bDirty = true;

private:
    // 컴포넌트·씬·외형의 비소유 참조
    UDecalComponent* Component = nullptr;
    FScene* Scene = nullptr;
    UMaterial* Material = nullptr;

    FMatrix DecalToWorld = FMatrix::Identity;
    FMatrix WorldToDecal = FMatrix::Identity;
    FBox Bounds{ FVector(0.0f), FVector(0.0f) };
    FVector Forward = FVector(1.0f, 0.0f, 0.0f);
    FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
    int32 SortOrder = 0;
    uint32 Id = 0;
    float AngleFadeStart = 60.0f;
    float AngleFadeEnd = 85.0f;
    float FadeAlpha = 1.0f;
    bool bValidTransform = false;
};
