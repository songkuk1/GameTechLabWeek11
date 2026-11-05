#pragma once
#include "HitResult.h"
#include "CollisionShape.h"
#include "Math/Rotator.h"
#include "Math/Box.h"

class AActor;
class UWorld;
class UPrimitiveComponent;

// 큐에 쌓일 이벤트
enum class EOverlapEventType : uint8 
{ 
    Begin, 
    End 
};

struct FPendingOverlapEvent
{
    EOverlapEventType Type;
    UPrimitiveComponent* Self;
    UPrimitiveComponent* Other;
};

struct FPendingHitEvent
{
    UPrimitiveComponent* Self;
    UPrimitiveComponent* Other;
    FHitResult Hit;
    bool bSelfMoved;
};

class FPhysicsScene
{
public:
    // 월드가 컴포넌트보다 먼저 지워져도 컴포넌트가 해제된 Scene을 가리키지 않게
    ~FPhysicsScene() { RemoveAllPrimitives(); }

    // 수집, 쌍 판정, 집합 비교, 목록 갱신, 큐 적재
    void UpdateOverlaps(UWorld& World);
        
    // 큐를 한꺼번에 발송하고 비움
    void DispatchPendingEvents();

    // 형상 있는 Primitive를 충돌 목록에 등록. FScene::AddPrimitive와 같은 곳에서 부른다
    void AddPrimitive(UPrimitiveComponent* Component);

    // 파괴 시 정리
    void RemovePrimitive(UPrimitiveComponent* Component);

    // 월드 정리 시 목록과 대기 이벤트를 통째로 비운다 (이벤트 발송 없음)
    void RemoveAllPrimitives();

    // 목록에서만 뺀다. 이벤트 발송 없음. Primitive 소멸자가 부른다
    void UnregisterPrimitive(UPrimitiveComponent* Component);

    // MoveComponent가 나중에 사용
    void QueueHit(const FPendingHitEvent& Event);

    // Mover를 Delta만큼 sweep했을 때 가장 먼저 막히는 Block 컴포넌트
    bool ComponentSweepSingle(UPrimitiveComponent * Mover, const FVector & Delta,
                                  const FRotator & Rot, FHitResult & OutHit);

    // Shape를 Delta만큼 sweep했을 때 막히는 Block 컴포넌트
	bool SweepSingleByShape(const UWorld& World, const FCollisionShape& Shape, const FVector& Start, const FVector& Delta,
		const FRotator& Rot, const AActor* IgnoreActor, FHitResult& OutHit);

private:
    // 이 월드의 형상 있는 Primitive. 순서는 보장하지 않는다 (제거 시 RemoveAtSwap)
    TArray<UPrimitiveComponent*> CollisionComponents;

    // UpdateOverlaps가 매 프레임 채우는 컴포넌트별 값. 용량을 재사용한다
    TArray<FCollisionShape> FrameShapes;
    TArray<FVector> FrameLocations;
    TArray<FRotator> FrameRotations;
    TArray<FBox> FrameBounds;
    TArray<TArray<UPrimitiveComponent*>> CurrentOverlapsArray;

    //Dispatch 할 Overlaps
    TArray<FPendingOverlapEvent> PendingOverlaps;
    TArray<FPendingHitEvent> PendingHits;

    //Dispatch 중인 Hit(복사용)
    TArray<FPendingHitEvent> DispatchingHits;
};