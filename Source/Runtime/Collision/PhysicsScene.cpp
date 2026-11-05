#include "EnginePCH.h"
#include "PhysicsScene.h"
#include "Engine/World.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SphereComponent.h"
#include "PhysicsInterface.h"

void FPhysicsScene::UpdateOverlaps(UWorld& World)
{
	//후보는 등록된 콜리전 컴포넌트 목록. SpawnActor, 레벨 로드나 복제, 컴포넌트 추가에서 등록된다.
	//판정 중에는 이벤트를 발송하지 않으므로 목록이 바뀌지 않는다.
	const TArray<UPrimitiveComponent*>& Candidates = CollisionComponents;
	const int32 Count = Candidates.Num();

	//콜리전에 2개 이상이어야 충돌 검사한다. -> 아님.
	//	마지막 하나 남은 콜리전 컴포넌트가 이전 프레임에 누군가와 오버랩 되어 있었다면 영원히 End가 오지 않는다.
	//	0, 1개여도 이전 목록과 비교는 해야한다.
	//if (SphereCollisionArray.Num() < 2) return;

	//형상, 위치, 회전은 쌍마다 다시 구하지 않도록 컴포넌트당 한 번만 구해 둔다.
	FrameShapes.SetNum(Count);
	FrameLocations.SetNum(Count);
	FrameRotations.SetNum(Count);
	FrameBounds.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FrameShapes[i] = Candidates[i]->GetCollisionShape();
		FrameLocations[i] = Candidates[i]->GetWorldLocation();
		FrameRotations[i] = Candidates[i]->GetWorldRotation();
		FrameBounds[i] = FPhysicsInterface::CalcShapeBounds(FrameShapes[i], FrameLocations[i], FrameRotations[i]);
	}

	//집합 목록 기반 Begin, End 검사

	//모든 목록을 순회하며 오버랩 목록을 만든다. 안쪽 배열은 용량을 재사용한다.
	CurrentOverlapsArray.SetNum(Count);
	for (TArray<UPrimitiveComponent*>& Overlaps : CurrentOverlapsArray)
		Overlaps.Reset();

	for (int i = 0; i < Count; ++i)
	{
		UPrimitiveComponent* CollisionCompA = Candidates[i];

		for (int j = i + 1; j < Count; ++j)
		{
			UPrimitiveComponent* CollisionCompB = Candidates[j];

			//두 콜리전 컴포넌트가 같은 액터에 속해있다면 오버랩 판정하지 않는다.
			//필요한 경우가 있을까? 옵션을 따로 둬서 1개 액터 내의 콜리전끼리도 이벤트 호출이 필요하다면 추가하자.
			if (CollisionCompA->GetOwner() == CollisionCompB->GetOwner()) continue;

			//Overlap 옵션 꺼져있으면 계산 안함
			if (!CollisionCompA->GetGenerateOverlapEvents() || !CollisionCompB->GetGenerateOverlapEvents()) continue;
			//둘 다 Block이면 오버랩 안함
			if (CollisionCompA->IsBlockComponent() && CollisionCompB->IsBlockComponent()) continue;

			//AABB가 떨어져 있으면 형상도 겹칠 수 없다. 정밀 판정 전에 먼저 거른다
			if (!FrameBounds[i].Intersects(FrameBounds[j])) continue;

			//충돌 테스트
			if (FPhysicsInterface::OverlapTest(FrameShapes[i], FrameLocations[i], FrameRotations[i],
											   FrameShapes[j], FrameLocations[j], FrameRotations[j]))
			{
				//이번 오버랩 목록 만들기
				CurrentOverlapsArray[i].Add(CollisionCompB);
				CurrentOverlapsArray[j].Add(CollisionCompA);
			}			
		}
	}

	//오버랩 집합 목록을 각 컴포넌트의 목록과 비교한다.
	for (int i = 0; i < Candidates.Num(); ++i)
	{
		UPrimitiveComponent* TargetComp = Candidates[i];

		//Target의 이전 오버랩 목록
		const TArray<UPrimitiveComponent*>& BeforeOverlaps = TargetComp->GetOverlapComps();

		//Target의 현재 오버랩 목록
		const TArray<UPrimitiveComponent*>& CurrentOverlaps = CurrentOverlapsArray[i];

		//이전 - 현재 : End
		//현재 - 이전 : Begin

		/*
		이전의 각 원소 p:  이번 목록에서 Find(p)가 없으면 → End(나, p)
		이번의 각 원소 c : 이전 목록에서 Find(c)가 없으면 → Begin(나, c)
		*/

		for (auto& BeforeComp : BeforeOverlaps)
		{
			//현재에 이전 콜리전이 존재하지 않는다면 : End
			if (CurrentOverlaps.Find(BeforeComp) == -1)
			{
				//Type, Self, Other
				PendingOverlaps.Add(FPendingOverlapEvent{ EOverlapEventType::End, TargetComp, BeforeComp });
			}
		}

		for (auto& CurrentComp : CurrentOverlaps)
		{
			//이전 콜리전에 현재 콜리전이 없다면 : Begin
			if (BeforeOverlaps.Find(CurrentComp) == -1)
			{
				//Type, Self, Other
				PendingOverlaps.Add(FPendingOverlapEvent{ EOverlapEventType::Begin, TargetComp, CurrentComp });
			}
		}
	}

	//Overlapping Components 최신으로 갱신
	for (int i = 0; i < Candidates.Num(); ++i)
	{
		UPrimitiveComponent* TargetComp = Candidates[i];

		//Target의 현재 오버랩 목록
		TArray<UPrimitiveComponent*>& CurrentOverlaps = CurrentOverlapsArray[i];

		TargetComp->SetOverlapComps(CurrentOverlaps);
	}
}

void FPhysicsScene::QueueHit(const FPendingHitEvent& Event)
{
	PendingHits.Add(Event);
}

bool FPhysicsScene::ComponentSweepSingle(UPrimitiveComponent* Mover, const FVector& Delta, const FRotator& Rot, FHitResult& OutHit)
{
	AActor* Owner = Mover->GetOwner();
	return SweepSingleByShape(*Owner->GetWorld(), Mover->GetCollisionShape(), Mover->GetWorldLocation(), Delta, Rot, Owner, OutHit);
}

bool FPhysicsScene::SweepSingleByShape(const UWorld& World, const FCollisionShape& Shape, const FVector& Start,
	const FVector& Delta, const FRotator& Rot, const AActor* IgnoreActor, FHitResult& OutHit)
{
	FHitResult Temp;
	FHitResult Best;
	Best.Time = 1.f;
	bool bFound = false;

	//이동 경로 전체를 감싸는 AABB. 회전은 Sweep 동안 그대로이므로 시작과 끝 박스를 합치면 된다
	FBox SweepBounds = FPhysicsInterface::CalcShapeBounds(Shape, Start, Rot);
	SweepBounds.Expand(FPhysicsInterface::CalcShapeBounds(Shape, Start + Delta, Rot));

	//이 PhysicsScene에 등록된 것만 돌므로 월드 검사는 필요 없다.
	for (UPrimitiveComponent* Comp : CollisionComponents)
	{
		if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
		if (!Comp->IsBlockComponent()) continue;

		//이동 경로와 AABB가 떨어져 있으면 닿을 수 없다
		const FCollisionShape CompShape = Comp->GetCollisionShape();
		const FVector CompLocation = Comp->GetWorldLocation();
		const FRotator CompRotation = Comp->GetWorldRotation();
		if (!SweepBounds.Intersects(FPhysicsInterface::CalcShapeBounds(CompShape, CompLocation, CompRotation))) continue;

		//Temp 초기화
		Temp = FHitResult();
		if (FPhysicsInterface::SweepTest(Shape, Start, Delta, Rot,
			CompShape, CompLocation, CompRotation, Temp))
		{
			//이거보다 가까울순 없어서 조기 종료
			if (Temp.Time == 0)
			{
				Best = Temp;
				Best.HitComponent = Comp;
				bFound = true;
				break;
			}

			//이번에 Temp 결과가 더 가깝다
			if (Temp.Time < Best.Time)
			{
				Best = Temp;
				Best.HitComponent = Comp;
				bFound = true;
			}
		}
	}

	OutHit = Best;
	return bFound;
}

void FPhysicsScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->RegisteredPhysicsScene) return;
	if (Component->GetCollisionShape().ShapeType == ECollisionShape::None) return;

	//Overlap 옵션이 꺼져 있어도 등록해야 한다.
	//	이전 프레임에서 Overlap 되어서 BeginOverlap 했고, 이벤트로 옵션을 껐을 때
	//	목록에 없으면 End overlap 호출이 불리지 않는다.
	//	오버랩 목록 만들 때 옵션 검사를 통해 오버랩 목록에 포함시키지 않도록 하고 End Overlap 호출이 불려야 한다.
	Component->RegisteredPhysicsScene = this;
	Component->PhysicsIndex = CollisionComponents.Num();
	CollisionComponents.Add(Component);
}

void FPhysicsScene::UnregisterPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->RegisteredPhysicsScene != this) return;

	const int32 Index = Component->PhysicsIndex;
	CollisionComponents.RemoveAtSwap(Index);
	if (Index < CollisionComponents.Num())
		CollisionComponents[Index]->PhysicsIndex = Index;

	Component->RegisteredPhysicsScene = nullptr;
	Component->PhysicsIndex = -1;
}

void FPhysicsScene::RemoveAllPrimitives()
{
	for (UPrimitiveComponent* Component : CollisionComponents)
	{
		Component->RegisteredPhysicsScene = nullptr;
		Component->PhysicsIndex = -1;
	}
	CollisionComponents.Reset();

	//지워질 컴포넌트를 가리키는 이벤트가 남지 않게
	PendingOverlaps.Reset();
	PendingHits.Reset();
}

void FPhysicsScene::DispatchPendingEvents()
{
	//DispatchingOverlaps로 옮겨서 Dispatch한다.
	//Dispatch 중에 콜리전 컴포넌트가 생성되거나 삭제된다면 순회하는 배열이 바뀌어 버린다.
	//이벤트 호출로 인한 새로운 콜리전 컴포넌트는 다음 프레임에 수집되어 처리될 수 있도록 한다.

	//굳이 필요 없지 않나??? 이미 콜리전 판정은 끝났고 PendingOverlaps에 영향을 줄 수 없다.
	/*DispatchingOverlaps = std::move(PendingOverlaps);
	PendingOverlaps.Reset();*/

	//Overlap
	//End를 먼저 보내고 Begin을 보내도록 두번 돈다
	//A영역을 나와서 B영역을 들어간다라는 동작의 순서를 보장
	for (auto& Overlap : PendingOverlaps)
	{
		if (Overlap.Type == EOverlapEventType::End)
		{
			if (Overlap.Self == nullptr || Overlap.Other == nullptr) continue;

			Overlap.Self->DispatchEndOverlap(Overlap.Other);
		}
	}

	for (auto& Overlap : PendingOverlaps)
	{
		if (Overlap.Type == EOverlapEventType::Begin)
		{
			if (Overlap.Self == nullptr || Overlap.Other == nullptr) continue;

			Overlap.Self->DispatchBeginOverlap(Overlap.Other);
		}
	}

	PendingOverlaps.Reset();

	//Hit
	//Hit은 오버랩과 달리 발송중에 Hit이 또 생길 수 있다.
	//루프를 돌때 추가되는 목록이 있지 않도록 옮겨서 사용한다.
	DispatchingHits = std::move(PendingHits);
	PendingHits.Reset();

	for (int i = 0; i < DispatchingHits.Num(); ++i)
	{
		FPendingHitEvent HitEvent = DispatchingHits[i];
		if (!HitEvent.Self || !HitEvent.Other) continue;
		HitEvent.Self->DispatchBlockingHit(HitEvent.Other, HitEvent.Hit, HitEvent.bSelfMoved);
	}

	DispatchingHits.Reset();
}

void FPhysicsScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component) return;

	UnregisterPrimitive(Component);
	if (Component->GetCollisionShape().ShapeType == ECollisionShape::None) return;

	//오버랩 목록을 지역 변수로 옮긴다.
	//순회하며 무슨 일이 일어나도 순회중인 배열에는 영향이 없도록
	const TArray<UPrimitiveComponent*> Overlaps = Component->TakeOverlapComps();

	//들고 있는 오버랩 목록 비우기
	/*TArray<UPrimitiveComponent*> Empty = {};
	Component->SetOverlapComps(Empty);*/

	//들고 있는 목록 순회하며 나를 제거해준다.
	//EndOverlap 이벤트 발송해준다.
	for (int32 i = 0; i < Overlaps.Num(); ++i)
	{
		UPrimitiveComponent* Comp = Overlaps[i];
		if (!Comp) continue;
		Comp->RemoveOverlapCompAndDispatchEndOverlap(Component);
	}

	/*부연 설명
	지연 파괴가 없었을 경우에는 아래와 같은 상황에서 문제가 생겼다.
	
	상황 1. 
	Begin 없이 End를 받음

	이번 프레임에 P와 C가 겹치기 시작 : Begin(P, C) 적재, P의 목록에는 이미 C가 들어감
	이벤트 발송 중, Begin이 전달되기 전에 다른 핸들러가 C를 파괴
	RemovePrimitive(C) → P의 목록에 C가 있으므로 RemoveOverlapComponent → P가 End를 즉시 받음
	Begin(P, C)는 null 처리되어 전달되지 않음
	→ P는 시작한 적 없는 겹침의 End를 받는다.

	상황2.
	Begin을 받았는데 End를 못 받음

	지난 프레임에 P가 C와의 Begin을 받음
	이번 프레임에 둘이 떨어짐 → End(P, C) 적재, P의 목록에서는 이미 C가 빠짐
	발송 중, 그 End가 전달되기 전에 다른 End 핸들러가 C를 파괴
	RemovePrimitive(C) → P의 목록에 C가 없으니 즉시 End를 보내지 않고, 큐의 End(P, C)는 null 처리됨
	→ P는 End를 영영 받지 못한다.
	
	이벤트 발송 중에 파괴가 발생할 경우 이러한 경우 같은 문제가 너무 많이 생기고 처리하기 까다롭다.
	지연 파괴를 적용시켜서 바로 목록에서도 제거하고 EndOverlap도 받게 한다.
	*/	

	//에디터 즉시 삭제 경로에서는 혹시 모를 상황을 위해 큐에 Self나 Other가 Component라면 이벤트 null처리
	for (int i = 0; i < PendingOverlaps.Num(); ++i)
	{
		if (PendingOverlaps[i].Self == Component || PendingOverlaps[i].Other == Component)
		{
			PendingOverlaps[i].Self = nullptr;
			PendingOverlaps[i].Other = nullptr;
		}
	}
}