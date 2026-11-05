#include "EnginePCH.h"

#include "Editor/Viewports/Viewport.h"
#include "Editor/Viewports/ViewportCameraMath.h"
#include "Camera/CameraTypes.h"

#include <algorithm>
#include <cmath>

using namespace ViewportCameraMath;

// 예전 패널 Slot과 같이 최소 1픽셀, 소수는 버린 정수 크기로 만든다.
void FViewport::EnsureRenderTargets()
{
    if (!IsVisible())
        return;
    Targets.Resize(
        static_cast<uint32>(std::max(1.0f, OutputSize.X)),
        static_cast<uint32>(std::max(1.0f, OutputSize.Y)));
}

void FViewport::UpdateInput(FViewportInput& Input)
{
    FViewportClient* Client = GetActiveClient();
    if (!Client || !IsVisible() || !Input.bEnabled)
        return;
    Client->UpdateInput(Input, GetOutputRect());
}

FViewportClient* FViewport::GetActiveClient() const
{
    if (GameClient)
        return GameClient.get();
    return EditorClient.get();
}

const FViewCamera& FViewport::GetCamera() const
{
    static const FViewCamera DefaultCamera{};
    const FViewportClient* Client = GetActiveClient();
    return Client ? Client->GetCamera() : DefaultCamera;
}

// 직교 화면의 XY는 유지하면서 렌더·컬링·피킹용 카메라만 후퇴시켜 양방향 깊이를 확보한다.
FViewCamera FViewport::GetRenderCamera() const
{
    FViewCamera Camera = GetCamera();
    if (Camera.Projection.Mode != EProjectionMode::Orthographic)
        return Camera;

    const float AspectRatio = OutputSize.X > 0.0f && OutputSize.Y > 0.0f
        ? OutputSize.X / OutputSize.Y : 1.0f;
    const float HalfWidth = Camera.Projection.OrthoWidth * 0.5f;
    const float HalfHeight = HalfWidth / AspectRatio;
    const FVector Position = Camera.Transform.Location;
    const float Distance = std::sqrt(
        Position.X * Position.X + Position.Y * Position.Y + Position.Z * Position.Z);

    // 현재 카메라 평면의 앞뒤를 모두 포함하고 기존 FarClip보다 깊이 범위를 줄이지 않는다.
    const float Radius = std::max(Camera.Projection.FarClip,
        2.0f * Distance + 4.0f * std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight));
    const FVector Forward = NormalizedOrZero(CameraForward(Camera.Transform.Rotation));
    const float Retreat = Radius + Camera.Projection.NearClip;
    Camera.Transform.Location = Position - Forward * Retreat;
    Camera.Projection.FarClip = Camera.Projection.NearClip + 2.0f * Radius;
    return Camera;
}

const FViewport::FPreparedView& FViewport::PrepareView() const
{
    const FViewCamera Camera = GetRenderCamera();
    Prepared.ViewProjection = BuildViewMatrix(Camera.Transform) *
        BuildProjectionMatrix(Camera.Projection, OutputSize.X / OutputSize.Y);
    Prepared.Frustum = ExtractFrustumPlanes(Prepared.ViewProjection);
    return Prepared;
}

FVector FViewport::GetCameraLocation() const
{
    return GetCamera().Transform.Location;
}

FVector FViewport::GetCameraForward() const
{
    return NormalizedOrZero(CameraForward(GetCamera().Transform.Rotation));
}

FVector FViewport::GetCameraUp() const
{
    return NormalizedOrZero(CameraUp(GetCamera().Transform.Rotation));
}

FMatrix FViewport::BuildBillboardMatrix(const FVector& WorldPosition, const float Width, const float Height) const
{
    const FCameraTransform& Camera = GetCamera().Transform;
    const FBillboardView View{
        .Location = Camera.Location,
        .Forward = NormalizedOrZero(CameraForward(Camera.Rotation)),
        .Right = NormalizedOrZero(CameraRight(Camera.Rotation)),
        .Up = NormalizedOrZero(CameraUp(Camera.Rotation))
    };

    return View.MakeMatrix(WorldPosition, Width, Height);
}

FRay FViewport::Deproject(const FVector2 ViewLocalPosition) const
{
    return ::Deproject(GetRenderCamera(), ViewLocalPosition, OutputSize);
}
