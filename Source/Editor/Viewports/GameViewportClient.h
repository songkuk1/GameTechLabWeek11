#pragma once

#include "Editor/Viewports/ViewportClient.h"

// PIE처럼 게임 쪽 카메라가 그리는 View의 Client. 카메라 값은 엔진이 매 프레임 넣어 준다.
// 에디터 카메라 입력은 받지 않는다. 게임 입력은 지금처럼 PlayerController가 직접 처리한다.
// UI를 그리는 UGameViewportClient(Runtime)와는 다른 클래스다.
class FGameViewportClient : public FViewportClient
{
public:
    explicit FGameViewportClient(UWorld& InWorld) : World(&InWorld) {}
    UWorld* GetWorld() const override { return World; }
    const FViewCamera& GetCamera() const override { return Camera; }
    // Forward는 정규화되어 있어야 하며 Roll은 버린다.
    void SetCamera(const FVector& Location, const FVector& Forward, const FCameraProjection& Projection, const FRect& Rect);

private:
    UWorld* World; // PlayWorld 비소유. 월드가 바뀌면 엔진이 Client를 새로 만든다.
    FViewCamera Camera{};
};
