#pragma once

#include "Editor/LevelEditor/MultipleViewports/Core/MultipleViewports.h"
#include "Render/RenderPacket.h"
#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Containers/Map.h"
#include "Containers/Array.h"

class FViewport;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UWorld;

// 월드 하나를 캡처해 두고, View마다 절두체 컬링·LOD 선택·빌보드 행렬로 렌더 큐를 만든다.
// 예전 MultipleViewportsAdapter의 캡처·렌더 큐 부분이다. 카메라는 FViewport에서 읽는다.
class FViewportRenderCollector
{
public:
    // Tick 뒤 현재 World의 ID·경계만 캡처한다. Mesh·삼각형은 복사하지 않는다.
    void CaptureWorld(UWorld& World, bool bDrawSkeletalMeshes);
    // 마지막으로 캡처한 World를 Viewport 카메라로 컬링해 렌더 큐를 구성한다. Viewport는 활성이어야 한다.
    void BuildRenderQueue(const FViewport& Viewport, FRenderQueue& OutQueue);
    // 마지막 BuildRenderQueue에서 절두체를 통과한 오브젝트 수를 반환한다.
    std::size_t GetVisibleObjectCount() const { return VisibleIds.Num(); }
    // 최근 World 스냅샷에 Billboard가 포함됐는지 반환한다.
    bool HasCapturedBillboard() const { return bCapturedBillboard; }
    // 최근 World 스냅샷에 Particle이 포함됐는지 반환한다.
    bool HasCapturedParticle() const { return bCapturedParticle; }

private:
    // 이번 프레임의 엔진 객체와 파티클 준비 상태를 보관한다. 포인터는 다음 캡처 전까지 유효해야 한다.
    struct PrimitiveSnapshot
    {
        UPrimitiveComponent* Primitive = nullptr;
        bool bCaptured = false;
        bool bParticlesPrepared = false;
        TArray<int32> AliveParticleIndices;
    };
    TMap<ObjectId, PrimitiveSnapshot> PrimitiveById;
    // 컬링 입력 버퍼를 소유하고 용량을 재사용한다.
    TArray<FRenderableObject> RenderObjects;
    // 불투명 파티클은 최종 렌더러가 거리 정렬하지 않아 기존 Core 정렬을 유지한다.
    TArray<FParticleSortInput> SortInputs;
    TArray<ObjectId> SortedParticleIds;
    TArray<ObjectId> VisibleIds;
    TArray<UStaticMeshComponent*> PendingStaticMeshes;
    TArray<FLODSelectionInput> LODInputs;
    TArray<uint8> SelectedLODs;
    bool bCapturedBillboard = false;
    bool bCapturedParticle = false;
};
