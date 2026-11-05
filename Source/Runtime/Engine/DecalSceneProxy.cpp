#include "EnginePCH.h"
#include "Engine/DecalSceneProxy.h"
#include "Component/DecalComponent.h"
#include "Render/Material.h"
#include <cmath>

// 등록된 컴포넌트 및 씬 참조
FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent, FScene* InScene)
    : Component(InComponent), Scene(InScene), Id(InComponent->GetUUID())
{
}

// 전체 크기 및 월드 변환 갱신
void FDecalSceneProxy::Refresh()
{
    bDirty = false;
    bValidTransform = false;
    Material = Component->GetDecalMaterial();
    Color = Component->GetDecalColor();
    SortOrder = Component->GetSortOrder();
    FadeAlpha = Component->GetFadeAlpha();

    const FVector& Size = Component->GetDecalSize();
    const FMatrix Scale(
        Size.X, 0, 0, 0,
        0, Size.Y, 0, 0,
        0, 0, Size.Z, 0,
        0, 0, 0, 1);
    const FMatrix World = Component->GetWorldMatrix();
    DecalToWorld = Scale * World;

    const float Determinant = DecalToWorld.Determinant();
    if (!std::isfinite(Determinant) || std::fabs(Determinant) <= 1.0e-12f)
    {
        WorldToDecal = FMatrix::Identity;
        Bounds = FBox{ FVector(0.0f), FVector(0.0f) };
        return;
    }

    WorldToDecal = DecalToWorld.Inverse();
    Bounds = FBox{ FVector(-0.5f), FVector(0.5f) }.GetWorldAABB(DecalToWorld);
    Forward = World.TransformVector(FVector(1.0f, 0.0f, 0.0f)).GetSafeNormal();
    bValidTransform = true;
}

// 현재 활성 상태 및 투영 유효성 검사
bool FDecalSceneProxy::IsVisible() const
{
    return Component->IsActive() && bValidTransform && Material &&
        Material->MaterialDomain == EMaterialDomain::Decal && FadeAlpha > 0.0f;
}
