#include "EnginePCH.h"

#include "Editor/Viewports/GameViewportClient.h"
#include "Editor/Viewports/ViewportCameraMath.h"

#include <cmath>

using namespace ViewportCameraMath;

// 외부 카메라의 위치·방향·투영을 보관한다.
void FGameViewportClient::SetCamera(const FVector& Location, const FVector& Forward,
    const FCameraProjection& Projection, const FRect& Rect)
{
    Camera.Transform.Location = Location;
    const FVector Direction = NormalizedOrZero(Forward);
    Camera.Transform.Rotation = MakeCameraRotation(
        std::atan2(Direction.Y, Direction.X) * 180.0f / Pi,
        std::asin(FMath::Clamp(Direction.Z, -1.0f, 1.0f)) * 180.0f / Pi);
    Camera.Projection = Projection;
    ConstrainOrthographicWidth(Camera.Projection, Rect);
}
