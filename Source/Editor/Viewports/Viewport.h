#pragma once

#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Viewports/GameViewportClient.h"

#include "Math/Frustum.h"
#include "Math/Matrix.h"
#include "Render/ViewMode.h"
#include "Render/SceneRenderTargets.h"

// 화면의 View 하나. 출력 크기·렌더 타깃·표시 모드를 갖고, 어느 Client의 카메라로 그릴지 고른다.
// 화면 안의 배치(위치)는 이 View를 담은 패널·창이 맡는다. 출력 크기만 넘겨받는다.
// 편집 Client는 계속 보관하고, PIE 중에는 Game Client가 대신 그린다(언리얼의 PIE 뷰포트 전환과 같다).
class FViewport
{
public:
    void SetEditorClient(TUniquePtr<FEditorViewportClient> Client) { EditorClient = std::move(Client); }
    FEditorViewportClient* GetEditorClient() const { return EditorClient.get(); }

    // 이 View를 Game Client의 카메라로 그린다. 편집 Client는 그대로 보관한다.
    void BeginPlay(TUniquePtr<FGameViewportClient> Client) { GameClient = std::move(Client); }
    // Game Client를 지우고 편집 Client로 되돌린다.
    void EndPlay() { GameClient.reset(); }
    FGameViewportClient* GetGameClient() const { return GameClient.get(); }
    bool HasGameClient() const { return GameClient != nullptr; }
    // 지금 이 View를 그리는 Client. Game Client가 있으면 그쪽이다.
    FViewportClient* GetActiveClient() const;

    // 담은 패널·창이 레이아웃을 계산할 때마다 갱신한다. 크기가 0이면 숨김이며 렌더 타깃은 유지한다.
    void SetOutputSize(FVector2 InSize) { OutputSize = InSize; }
    // 픽셀 크기로 받는 형태. 창·패널처럼 정수 크기를 아는 쪽이 쓴다.
    void SetOutputSize(uint32 Width, uint32 Height) { OutputSize = {static_cast<float>(Width), static_cast<float>(Height)}; }
    FVector2 GetOutputSize() const { return OutputSize; }
    bool IsVisible() const { return OutputSize.X > 0.0f && OutputSize.Y > 0.0f; }
    // 원점 기준 출력 사각형. 카메라 계산 함수가 받는 FRect 형식이다.
    FRect GetOutputRect() const { return {0.0f, 0.0f, OutputSize.X, OutputSize.Y}; }

    // 출력 크기에 맞춘 렌더 타깃 준비 및 동일 크기의 기존 자원 재사용
    void EnsureRenderTargets();
    // 렌더 타깃 해제 및 다음 준비 시 재생성
    void ReleaseRenderTargets() { Targets.Release(); }
    // 화면에 표시할 최종 결과 텍스처 반환
    FTexture2D* GetColorTarget() const { return Targets.ViewColor.get(); }
    // 화면 표시용 타깃과 같은 크기·포맷의 FXAA 임시 텍스처 반환
    FTexture2D* GetFxaaTarget() const { return Targets.FxaaTemp.get(); }
    // DBuffer를 포함한 View별 렌더 타깃 묶음 반환
    const FSceneRenderTargets& GetRenderTargets() const { return Targets; }

    // 활성 Client에 입력을 넘긴다. 숨겨졌거나 Input.bEnabled가 꺼져 있으면 넘기지 않는다.
    // Client가 바꾼 MoveSpeed는 Input에 남는다.
    void UpdateInput(FViewportInput& Input);

    void SetViewRenderMode(const FViewRenderMode& Value) { ViewRenderMode = Value; }
    FViewRenderMode GetViewRenderMode() const { return ViewRenderMode; }

    // 활성 Client의 카메라. Client가 없으면 기본값이다.
    const FViewCamera& GetCamera() const;
    // 직교 View의 논리 위치는 유지하고 렌더·컬링·피킹용 깊이 범위만 확장한 카메라.
    FViewCamera GetRenderCamera() const;

    // 현재 호출에서 계산한 행렬·절두체.
    struct FPreparedView
    {
        FFrustumPlanes Frustum{};
        FMatrix ViewProjection{};
    };
    // 호출할 때마다 현재 카메라로 VP와 절두체를 계산한다.
    const FPreparedView& PrepareView() const;

    FVector GetCameraLocation() const;
    FVector GetCameraForward() const;
    FVector GetCameraUp() const;
    // 카메라를 향하는 Billboard 월드 행렬을 만든다.
    FMatrix BuildBillboardMatrix(const FVector& WorldPosition, float Width, float Height) const;
    bool IsOrthographic() const { return GetCamera().Projection.Mode == EProjectionMode::Orthographic; }
    // 편집 Client의 프리셋과 실제로 그리는 카메라 방향으로 Grid 평면을 고른다. 편집 Client가 있어야 한다.
    EGridPlane GetGridPlane() const { return EditorClient->GetGridPlane(GetCamera().Transform.Rotation); }
    const FCameraProjection& GetProjection() const { return GetCamera().Projection; }
    // 출력 기준 로컬 좌표(픽셀)를 렌더 카메라로 역투영한다.
    FRay Deproject(FVector2 ViewLocalPosition) const;

private:
    TUniquePtr<FEditorViewportClient> EditorClient;
    TUniquePtr<FGameViewportClient> GameClient;
    // 레이아웃이 준 크기를 그대로 쓴다(소수 포함). 렌더 타깃만 정수 픽셀로 맞춘다.
    FVector2 OutputSize{};
    FSceneRenderTargets Targets;
    FViewRenderMode ViewRenderMode{};
    mutable FPreparedView Prepared{};
};
