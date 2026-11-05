#include "EnginePCH.h"
#include "CameraComponent.h"

#include "CameraTypes.h"

FMinimalViewInfo UCameraComponent::GetCameraView() const
{
    return {
        .Location = GetWorldLocation(),
        .Rotation = GetWorldRotation(),
        .FOV = FieldOfView,
        .AspectRatio = AspectRatio,
		.bConstrainAspectRatio = bConstrainAspectRatio,
        .OrthoWidth = OrthoWidth,
        .NearClip = NearClipPlane,
        .FarClip = FarClipPlane,
        .bOrthographic = bIsOrthogonal
    };
}
