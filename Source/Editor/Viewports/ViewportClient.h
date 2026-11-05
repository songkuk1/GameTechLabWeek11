#pragma once

#include "Editor/LevelEditor/MultipleViewports/Core/MultipleViewports.h"

class UWorld;

// 한 프레임 동안 View 하나에 전달하는 입력값의 묶음. 이벤트 처리나 중계 책임은 갖지 않는다.
// View를 담은 패널·창이 Capture·Hover를 채워 FViewport::UpdateInput으로 넘긴다.
struct FViewportInput
{
    // 출력 기준 로컬 마우스 좌표(픽셀).
    FVector2 MousePosition{};
    float DeltaTime = 0.0f;
    float MouseSensitivity = 0.0f;
    // 우클릭 + 휠로 바뀌는 카메라 이동 속도. Client가 바꾼 값을 호출자가 다시 읽는다.
    float MoveSpeed = 0.0f;
    int32 WheelDelta = 0;
    // 이 View가 우클릭 Capture를 갖고 있다. 실제 카메라 조작은 우클릭이 눌려 있을 때만 한다.
    bool bCaptured = false;
    // 마우스가 이 View 위에 있다.
    bool bPointerOver = false;
    // 이 View가 이번 프레임 입력을 받는다. 꺼져 있으면 Client에 넘기지 않는다.
    bool bEnabled = false;
};

// View 하나가 그릴 월드와 카메라를 제공한다. 출력 크기와 렌더 타깃은 FViewport가 맡는다.
class FViewportClient
{
public:
    virtual ~FViewportClient() = default;
    // 이 Client가 그리는 월드. 소유하지 않는다.
    virtual UWorld* GetWorld() const = 0;
    virtual const FViewCamera& GetCamera() const = 0;
    // 이 View에 전달된 입력으로 카메라를 갱신한다. 기본은 입력을 받지 않는다.
    virtual void UpdateInput(FViewportInput& Input, const FRect& OutputRect) {}
    // View 출력 크기가 유효한 크기로 바뀌었을 때 투영값을 보정한다.
    virtual void OnRectChanged(const FRect& Rect) {}
};
