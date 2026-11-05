#pragma once

#include "Editor/Viewports/ViewportClient.h"
#include "Editor/Viewports/ViewportTypes.h"

// 편집 View 하나의 카메라와 프리셋을 소유한다. View 선택과 Capture는 View를 담은 패널이 맡는다.
class FEditorViewportClient : public FViewportClient
{
public:
    // 편집할 월드를 지정한다. 레벨 View는 EditorWorld, Preview View는 그 탭의 Preview 월드다.
    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const override { return World; }
    const FViewCamera& GetCamera() const override { return Camera; }

    // 기본 View 구성에서 카메라와 프리셋을 그대로 지정한다.
    void InitializeCamera(const FViewCamera& InCamera, EMultipleViewportsCameraPreset InPreset);
    // 외부 카메라 값을 복사하고 투영 모드에 맞는 프리셋을 설정한다.
    void SetCamera(const FViewCamera& InCamera, const FRect& Rect);
    // 속성 창의 변경을 반영하되 위치·투영 수치만 바뀌면 기존 축 정렬 프리셋을 유지한다.
    void ApplyCameraProperties(const FViewCamera& InCamera, bool bRotationChanged, const FRect& Rect);
    // 선택한 프리셋의 위치·방향·투영 모드를 적용한다.
    void ApplyCameraPreset(EMultipleViewportsCameraPreset InPreset, const FRect& Rect);
    EMultipleViewportsCameraPreset GetCameraPreset() const { return Preset; }
    // 프리셋 또는 실제로 그리는 카메라 회전의 Forward 지배축으로 Grid 평면을 고른다.
    EGridPlane GetGridPlane(const FQuat& EffectiveRotation) const;
    // 카메라 회전은 그대로 두고, 중심·반지름으로 감싼 영역이 화면에 꽉 차지 않게 들어오도록 카메라를 옮긴다.
    void FocusOn(const FVector& Center, float Radius, const FRect& Rect);

    // Perspective는 이동·회전, Ortho는 화면 평면 드래그·폭 줌을 적용한다.
    // Capture 중 휠은 줌 대신 Input.MoveSpeed를 바꾼다.
    void UpdateInput(FViewportInput& Input, const FRect& Rect) override;
    void OnRectChanged(const FRect& Rect) override;

private:
    UWorld* World = nullptr; // 비소유
    FViewCamera Camera{};
    EMultipleViewportsCameraPreset Preset = EMultipleViewportsCameraPreset::Perspective;
};
