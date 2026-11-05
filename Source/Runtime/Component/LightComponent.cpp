#include "EnginePCH.h"
#include "LightComponent.h"
#include "Engine/LightSceneProxy.h"

FMatrix ULightComponent::GetViewMatrix() const
{
    return GetViewMatrix(GetWorldRotation());
}

FMatrix ULightComponent::GetViewMatrix(FRotator Rotation) const
{
    FVector WorldLocation = GetWorldLocation();
    FRotator WorldRotation = Rotation;

    FQuat WorldQuat = WorldRotation.Quaternion();
    FMatrix RotationMatrix = WorldQuat.ToFMatrix();

    FMatrix InverseTranslation = FMatrix::MakeTranslation(WorldLocation * -1.0f);
    FMatrix InverseRotation = RotationMatrix.GetTransposed();

    return InverseTranslation * InverseRotation;
}

FMatrix ULightComponent::GetPerspectiveMatrix(float FarClipPlane, float FieldOfView, float AspectRatio) const
{
    const float HalfFOV = FMath::DegreesToRadians(FieldOfView) * 0.5f;

    const float YScale = 1.0f / tan(HalfFOV);
    const float XScale = YScale / AspectRatio;

    const float ZScale = FarClipPlane / (FarClipPlane - NearClipPlane);
    const float ZOffset = -NearClipPlane * FarClipPlane / (FarClipPlane - NearClipPlane);

    //Forward가 X인 좌표
    return FMatrix(
        0.0f, 0.0f, ZScale, 1.0f,
        XScale, 0.0f, 0.0f, 0.0f,
        0.0f, YScale, 0.0f, 0.0f,
        0.0f, 0.0f, ZOffset, 0.0f
    );
}

FMatrix ULightComponent::GetOrthogonalMatrix(float FarClipPlane, float AspectRatio) const
{
    const float Width = OrthoWidth;
    const float Height = Width / AspectRatio;

    const float XScale = 2.0f / Width;
    const float YScale = 2.0f / Height;

    const float ZScale = 1.0f / (FarClipPlane - NearClipPlane);
    const float ZOffset = -NearClipPlane / (FarClipPlane - NearClipPlane);

    return FMatrix(
        0.0f, 0.0f, ZScale, 0.0f,
        XScale, 0.0f, 0.0f, 0.0f,
        0.0f, YScale, 0.0f, 0.0f,
        0.0f, 0.0f, ZOffset, 1.0f
    );
}

FMatrix ULightComponent::GetProjectionMatrix(bool bIsOrthogonal, float FarClipPlane, float FieldOfView, float AspectRatio) const
{
    if (bIsOrthogonal)
        return GetOrthogonalMatrix(FarClipPlane, AspectRatio);  // 직교 투영

    return GetPerspectiveMatrix(FarClipPlane, FieldOfView, AspectRatio);     // 원근 투영
}

FMatrix ULightComponent::GetViewProjectionMatrix(bool bIsOrthogonal, float FarClipPlane, float FieldOfView, float AspectRatio) const
{
    return GetViewMatrix() * GetProjectionMatrix(bIsOrthogonal, FarClipPlane, FieldOfView, AspectRatio);
}

FMatrix ULightComponent::GetViewProjectionMatrix(FRotator Rotation, bool bIsOrthogonal, float FarClipPlane, float FieldOfView, float AspectRatio) const
{
    return GetViewMatrix(Rotation) * GetProjectionMatrix(bIsOrthogonal, FarClipPlane, FieldOfView, AspectRatio);
}

FShadowInfo ULightComponent::GetShadowInfo() const
{
    return FShadowInfo();
}


ID3D11ShaderResourceView* ULightComponent::GetShadowPreviewSRV(int32 Index) const
{
    return GetSceneProxy() ? GetSceneProxy()->GetShadowPreviewSRV(Index) : nullptr;
}

int32 ULightComponent::GetShadowInfoNum() const
{
    if (GetSceneProxy() == nullptr) return 0;

    return GetSceneProxy()->GetShadowInfoNum();
}

void ULightComponent::SetPreviewShadowMapFlag(bool InShow)
{
    if (GetSceneProxy() == nullptr) return;

    GetSceneProxy()->SetPreviewShadowMapFlag(InShow);
}
