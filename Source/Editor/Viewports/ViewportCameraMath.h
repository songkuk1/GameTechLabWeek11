#pragma once

#include "Editor/LevelEditor/MultipleViewports/Core/MultipleViewports.h"
#include "Input/InputSystem.h"

#include <algorithm>
#include <cmath>

// Viewport와 ViewportClient가 함께 쓰는 카메라 계산. MultipleViewportsAdapter에서 그대로 옮겼다.
namespace ViewportCameraMath
{
inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float MinimumOrthographicWidth = 0.01f;
// 양방향 깊이 보정 뒤에도 절두체 평면 법선이 Core epsilon(1e-6)보다 충분히 크게 유지되게 한다.
// 정사각 View의 최악 조건에서 깊이 계수는 약 1 / (4 * sqrt(2) * Span)이다.
inline constexpr float MaximumOrthographicSpan = 80000.0f;

// 외부 설정과 화면 비율을 반영해 직교 투영 폭이 안전한 범위를 벗어나지 않게 한다.
inline void ConstrainOrthographicWidth(FCameraProjection& Projection, const FRect& Rect)
{
    const float AspectRatio = Rect.Width > 0.0f && Rect.Height > 0.0f
        ? Rect.Width / Rect.Height : 1.0f;
    // 폭과 높이 모두 상한 안에 들도록 세로로 긴 View에서는 폭 상한도 함께 줄인다.
    const float MaximumWidth = std::max(MinimumOrthographicWidth,
        MaximumOrthographicSpan * std::min(1.0f, AspectRatio));
    Projection.OrthoWidth = std::isnan(Projection.OrthoWidth)
        ? 10.0f
        : FMath::Clamp(Projection.OrthoWidth, MinimumOrthographicWidth, MaximumWidth);
}

// 양·음 방향 키 상태 차이로 -1~1 축 입력을 만든다.
inline float AxisValue(const EKeyCode Positive, const EKeyCode Negative)
{
    return (FInputSystem::IsKeyDown(Positive) ? 1.0f : 0.0f) - (FInputSystem::IsKeyDown(Negative) ? 1.0f : 0.0f);
}

// 길이가 0이면 영벡터를, 아니면 정규화 벡터를 반환한다.
inline FVector NormalizedOrZero(const FVector Value)
{
    const float LengthSquared = Value.X * Value.X + Value.Y * Value.Y + Value.Z * Value.Z;
    if (LengthSquared <= 0.0f) return {};
    const float InverseLength = 1.0f / std::sqrt(LengthSquared);
    return {Value.X * InverseLength, Value.Y * InverseLength, Value.Z * InverseLength};
}

// quaternion으로 회전된 카메라 Forward의 Z 성분을 계산한다.
inline float CameraForwardZ(const FQuat& Rotation)
{
    return 2.0f * (Rotation.X * Rotation.Z - Rotation.W * Rotation.Y);
}

// +X Forward를 quaternion으로 회전해 카메라 월드 Forward를 구한다.
inline FVector CameraForward(const FQuat& Rotation)
{
    return {
        1.0f - 2.0f * (Rotation.Y * Rotation.Y + Rotation.Z * Rotation.Z),
        2.0f * (Rotation.X * Rotation.Y + Rotation.W * Rotation.Z),
        2.0f * (Rotation.X * Rotation.Z - Rotation.W * Rotation.Y)};
}

// +Y Right를 quaternion으로 회전해 카메라 화면의 수평 월드축을 구한다.
inline FVector CameraRight(const FQuat& Rotation)
{
    return {
        2.0f * (Rotation.X * Rotation.Y - Rotation.W * Rotation.Z),
        1.0f - 2.0f * (Rotation.X * Rotation.X + Rotation.Z * Rotation.Z),
        2.0f * (Rotation.Y * Rotation.Z + Rotation.W * Rotation.X)};
}

// +Z Up을 quaternion으로 회전해 카메라 화면의 수직 월드축을 구한다.
inline FVector CameraUp(const FQuat& Rotation)
{
    return {
        2.0f * (Rotation.X * Rotation.Z + Rotation.W * Rotation.Y),
        2.0f * (Rotation.Y * Rotation.Z - Rotation.W * Rotation.X),
        1.0f - 2.0f * (Rotation.X * Rotation.X + Rotation.Y * Rotation.Y)};
}

// 카메라 Forward의 XY 방향을 atan2로 바꿔 Euler Yaw를 구한다.
inline float CameraYawDegrees(const FQuat& Rotation)
{
    const FVector Forward = NormalizedOrZero(CameraForward(Rotation));
    return std::atan2(Forward.Y, Forward.X) * 180.0f / Pi;
}

// 카메라 Forward의 Z 성분을 asin해 Euler Pitch를 구한다.
inline float CameraPitchDegrees(const FQuat& Rotation)
{
    return std::asin(FMath::Clamp(CameraForwardZ(Rotation), -1.0f, 1.0f)) * 180.0f / Pi;
}

// Z-up Yaw와 Right축 Pitch만 합성해 Roll 없는 quaternion을 만든다.
inline FQuat MakeCameraRotation(const float YawDegrees, const float PitchDegrees)
{
    const float HalfYaw = YawDegrees * Pi / 360.0f;
    const float HalfPitch = PitchDegrees * Pi / 360.0f;
    const float SinYaw = std::sin(HalfYaw);
    const float CosYaw = std::cos(HalfYaw);
    const float SinPitch = std::sin(HalfPitch);
    const float CosPitch = std::cos(HalfPitch);

    // 카메라 입력은 Euler Yaw/Pitch 두 축으로만 계산한다. Core 경계에 전달할 때만 quaternion으로 변환한다.
    return {
        SinPitch * SinYaw,
        -SinPitch * CosYaw,
        CosPitch * SinYaw,
        CosPitch * CosYaw};
}
} // namespace ViewportCameraMath
