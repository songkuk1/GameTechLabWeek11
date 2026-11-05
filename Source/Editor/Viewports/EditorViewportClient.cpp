#include "EnginePCH.h"

#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Viewports/ViewportCameraMath.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Input/InputSystem.h"

#include <algorithm>
#include <cmath>

using namespace ViewportCameraMath;

namespace
{
constexpr float MaximumCameraPitchDegrees = 89.0f;
constexpr float MaximumRotationDegreesPerFrame = 45.0f;
constexpr float MouseWheelDeltaPerStep = 120.0f;
constexpr float OrthographicZoomFactorPerStep = 0.9f;
constexpr float CameraMoveSpeedFactorPerStep = 1.25f;
// F 포커스에서 대상이 화면 가장자리에 붙지 않도록 남기는 여유 배율과 최소 반지름
constexpr float FocusMargin = 1.2f;
constexpr float MinimumFocusRadius = 0.1f;
}

void FEditorViewportClient::InitializeCamera(const FViewCamera& InCamera, const EMultipleViewportsCameraPreset InPreset)
{
    Camera = InCamera;
    Preset = InPreset;
}

// 카메라를 복사하고 Perspective이면 Forward 기준 Yaw·Pitch로 Roll을 제거한다.
void FEditorViewportClient::SetCamera(const FViewCamera& InCamera, const FRect& Rect)
{
    Camera = InCamera;
    ConstrainOrthographicWidth(Camera.Projection, Rect);
    if (Camera.Projection.Mode == EProjectionMode::Perspective)
        Camera.Transform.Rotation = MakeCameraRotation(
            CameraYawDegrees(Camera.Transform.Rotation),
            CameraPitchDegrees(Camera.Transform.Rotation));
    Preset = InCamera.Projection.Mode == EProjectionMode::Perspective
        ? EMultipleViewportsCameraPreset::Perspective
        : EMultipleViewportsCameraPreset::OrthographicView;
}

// 속성 편집을 적용하고 방향·투영 모드가 그대로이면 축 정렬 프리셋을 보존한다.
void FEditorViewportClient::ApplyCameraProperties(const FViewCamera& InCamera, const bool bRotationChanged, const FRect& Rect)
{
    const EMultipleViewportsCameraPreset PreviousPreset = Preset;
    const bool bKeepPreset = !bRotationChanged
        && Camera.Projection.Mode == InCamera.Projection.Mode;
    SetCamera(InCamera, Rect);
    if (bKeepPreset)
        Preset = PreviousPreset;
}

// 축 정렬 프리셋은 고정 Transform을, Current는 현재 Transform을 유지해 적용한다.
void FEditorViewportClient::ApplyCameraPreset(const EMultipleViewportsCameraPreset InPreset, const FRect& Rect)
{
    ConstrainOrthographicWidth(Camera.Projection, Rect);
    Preset = InPreset;
    Camera.Projection.Mode = InPreset == EMultipleViewportsCameraPreset::Perspective
        ? EProjectionMode::Perspective
        : EProjectionMode::Orthographic;

    switch (InPreset)
    {
    case EMultipleViewportsCameraPreset::Top:
        Camera.Transform = {{0.0f, 0.0f, 10.0f}, {0.0f, 0.70710678f, 0.0f, 0.70710678f}};
        break;
    case EMultipleViewportsCameraPreset::Bottom:
        Camera.Transform = {{0.0f, 0.0f, -10.0f}, {0.0f, -0.70710678f, 0.0f, 0.70710678f}};
        break;
    case EMultipleViewportsCameraPreset::Front:
        Camera.Transform = {{10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};
        break;
    case EMultipleViewportsCameraPreset::Back:
        Camera.Transform = {{-10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
        break;
    case EMultipleViewportsCameraPreset::Left:
        Camera.Transform = {{0.0f, -10.0f, 0.0f}, {0.0f, 0.0f, 0.70710678f, 0.70710678f}};
        break;
    case EMultipleViewportsCameraPreset::Right:
        Camera.Transform = {{0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, -0.70710678f, 0.70710678f}};
        break;
    case EMultipleViewportsCameraPreset::Perspective:
        Camera.Transform.Rotation = MakeCameraRotation(
            CameraYawDegrees(Camera.Transform.Rotation),
            CameraPitchDegrees(Camera.Transform.Rotation));
        break;
    case EMultipleViewportsCameraPreset::OrthographicView:
        // 투영 방식만 바꾸는 프리셋은 현재 Transform을 의도적으로 유지한다.
        break;
    }
}

// 축 정렬 프리셋은 고정 평면을, Current는 Forward 지배축에 수직인 평면을 고른다.
EGridPlane FEditorViewportClient::GetGridPlane(const FQuat& EffectiveRotation) const
{
    switch (Preset)
    {
    case EMultipleViewportsCameraPreset::Top:
    case EMultipleViewportsCameraPreset::Bottom:
        return EGridPlane::XY;
    case EMultipleViewportsCameraPreset::Front:
    case EMultipleViewportsCameraPreset::Back:
        return EGridPlane::YZ;
    case EMultipleViewportsCameraPreset::Left:
    case EMultipleViewportsCameraPreset::Right:
        return EGridPlane::XZ;
    case EMultipleViewportsCameraPreset::Perspective:
        return EGridPlane::XY;
    case EMultipleViewportsCameraPreset::OrthographicView:
        break;
    }

    const FVector Forward = CameraForward(EffectiveRotation);
    const float AbsX = std::fabs(Forward.X);
    const float AbsY = std::fabs(Forward.Y);
    const float AbsZ = std::fabs(Forward.Z);
    if (AbsZ >= AbsX && AbsZ >= AbsY) return EGridPlane::XY;
    if (AbsY >= AbsX) return EGridPlane::XZ;
    return EGridPlane::YZ;
}

// 원근은 세로·가로 FOV 중 좁은 쪽에 구가 들어오는 거리로, 직교는 구를 담는 폭으로 맞춘다.
void FEditorViewportClient::FocusOn(const FVector& Center, const float Radius, const FRect& Rect)
{
    const FVector Forward = NormalizedOrZero(CameraForward(Camera.Transform.Rotation));
    const float FocusRadius = std::max(Radius, MinimumFocusRadius) * FocusMargin;
    const float AspectRatio = Rect.Width > 0.0f && Rect.Height > 0.0f ? Rect.Width / Rect.Height : 1.0f;

    if (Camera.Projection.Mode == EProjectionMode::Orthographic)
    {
        // 폭과 높이(폭 / 비율) 모두 지름 이상이 되도록 한다. 직교는 거리와 무관하게 크기가 같다.
        Camera.Projection.OrthoWidth = 2.0f * FocusRadius * std::max(1.0f, AspectRatio);
        ConstrainOrthographicWidth(Camera.Projection, Rect);
        Camera.Transform.Location = Center - Forward * (2.0f * FocusRadius);
        return;
    }

    // FovDegrees는 세로 FOV다. 가로가 더 좁은 세로로 긴 View는 가로 FOV를 쓴다.
    const float HalfVertical = Camera.Projection.FovDegrees * Pi / 360.0f;
    const float HalfHorizontal = std::atan(std::tan(HalfVertical) * AspectRatio);
    const float HalfFov = std::min(HalfVertical, HalfHorizontal);
    const float Distance = std::max(FocusRadius / std::sin(HalfFov), Camera.Projection.NearClip + FocusRadius);
    Camera.Transform.Location = Center - Forward * Distance;
}

void FEditorViewportClient::UpdateInput(FViewportInput& Input, const FRect& Rect)
{
    constexpr float PerspectiveWheelSpeed = 0.1f;
    FCameraMoveInput MoveInput{};
    const bool bCameraCaptured = FInputSystem::IsMouseDown(EMouseButton::Right) && Input.bCaptured;
    const int WheelDelta = Input.WheelDelta;
    // 우클릭으로 카메라를 조작하는 동안 휠은 줌 대신 이동 속도를 배율로 바꾼다.
    if (bCameraCaptured && WheelDelta != 0)
    {
        const float WheelSteps = static_cast<float>(WheelDelta) / MouseWheelDeltaPerStep;
        Input.MoveSpeed = FMath::Clamp(
            Input.MoveSpeed * std::pow(CameraMoveSpeedFactorPerStep, WheelSteps),
            MinimumCameraMoveSpeed,
            MaximumCameraMoveSpeed);
    }
    const float MoveSpeed = Input.MoveSpeed;
    // 휠은 Capture 중이 아닐 때, 마우스 아래의 선택 View에만 줌으로 전달한다.
    const int ZoomWheelDelta = !bCameraCaptured && Input.bPointerOver ? WheelDelta : 0;
    const float ForwardAxis = bCameraCaptured ? AxisValue(EKeyCode::W, EKeyCode::S) : 0.0f;
    const float RightAxis = bCameraCaptured ? AxisValue(EKeyCode::D, EKeyCode::A) : 0.0f;
    const float VerticalAxis = bCameraCaptured ? AxisValue(EKeyCode::E, EKeyCode::Q) : 0.0f;
    const bool bOrthographic = Camera.Projection.Mode == EProjectionMode::Orthographic;
    // 직교 카메라 이동은 화면 평면으로 제한한다.
    // A/D는 수평, W/S는 수직으로 이동한다. 선택한 축 정렬 View가 뒤집히거나
    // 깊이축을 따라 이동하지 않도록 Q/E와 마우스 회전은 의도적으로 무시한다.
    const FVector Axis = bOrthographic
        ? NormalizedOrZero({0.0f, RightAxis, ForwardAxis})
        : NormalizedOrZero({ForwardAxis, RightAxis, VerticalAxis});
    MoveInput.MoveAxis = {Axis.X * MoveSpeed, Axis.Y * MoveSpeed, Axis.Z * MoveSpeed};

    bool bUpdateEulerRotation = false;
    float UpdatedYawDegrees = 0.0f;
    float UpdatedPitchDegrees = 0.0f;
    if (!bOrthographic && bCameraCaptured)
    {
        const FQuat& CurrentRotation = Camera.Transform.Rotation;
        const float CurrentYawDegrees = CameraYawDegrees(CurrentRotation);
        const float CurrentPitchDegrees = CameraPitchDegrees(CurrentRotation);
        const float YawDeltaDegrees = FMath::Clamp(
            FInputSystem::GetMouseDeltaX() * Input.MouseSensitivity,
            -MaximumRotationDegreesPerFrame,
            MaximumRotationDegreesPerFrame);
        const float PitchDeltaDegrees = FMath::Clamp(
            -FInputSystem::GetMouseDeltaY() * Input.MouseSensitivity,
            -MaximumRotationDegreesPerFrame,
            MaximumRotationDegreesPerFrame);
        UpdatedYawDegrees = CurrentYawDegrees + YawDeltaDegrees;
        UpdatedPitchDegrees = FMath::Clamp(
            CurrentPitchDegrees + PitchDeltaDegrees,
            -MaximumCameraPitchDegrees,
            MaximumCameraPitchDegrees);
        bUpdateEulerRotation = true;
    }
    if (bOrthographic)
    {
        // 패널 밖이나 다른 View 위의 휠 입력은 마지막 선택 카메라에 전달하지 않는다.
        const float WheelSteps = static_cast<float>(ZoomWheelDelta) / MouseWheelDeltaPerStep;
        Camera.Projection.OrthoWidth = std::max(
            MinimumOrthographicWidth,
            Camera.Projection.OrthoWidth *
                std::pow(OrthographicZoomFactorPerStep, WheelSteps));
        // 큰 휠 입력과 overflow도 동일한 안전 범위의 최댓값으로 제한한다.
        ConstrainOrthographicWidth(Camera.Projection, Rect);
    }
    else
    {
        MoveInput.ZoomDelta = static_cast<float>(ZoomWheelDelta) * PerspectiveWheelSpeed;
    }
    FViewCamera UpdatedCamera = ApplyCameraMovement(Camera, MoveInput, Input.DeltaTime);
    if (bOrthographic && bCameraCaptured)
    {
        const float WorldUnitsPerPixel = Camera.Projection.OrthoWidth / Rect.Width;
        const FVector Right = NormalizedOrZero(CameraRight(Camera.Transform.Rotation));
        const FVector Up = NormalizedOrZero(CameraUp(Camera.Transform.Rotation));
        // 화면을 잡아 끄는 방향으로 보이도록 카메라는 수평 드래그의 반대, 수직 드래그의 Up 방향으로 이동한다.
        const FVector PanOffset = Right * (-static_cast<float>(FInputSystem::GetMouseDeltaX()) * WorldUnitsPerPixel)
            + Up * (static_cast<float>(FInputSystem::GetMouseDeltaY()) * WorldUnitsPerPixel);
        UpdatedCamera.Transform.Location += PanOffset;
    }
    if (bUpdateEulerRotation)
        UpdatedCamera.Transform.Rotation = MakeCameraRotation(UpdatedYawDegrees, UpdatedPitchDegrees);
    Camera = UpdatedCamera;
}

// 창 크기와 Single/Quad 전환으로 Aspect가 바뀌어도 저장값과 실제 투영값을 함께 보정한다.
void FEditorViewportClient::OnRectChanged(const FRect& Rect)
{
    ConstrainOrthographicWidth(Camera.Projection, Rect);
}
