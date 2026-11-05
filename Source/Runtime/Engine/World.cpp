#include "EnginePCH.h"
#include "World.h"
#include "Level.h"

#include "Core/EngineStatics.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerStart.h"

#include "Input/InputSystem.h"

#include "UObject/UObjectIterator.h"

#include "Collision/Ray.h"
#include "Component/BillboardComponent.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "Component/LightComponentBase.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Math/Frustum.h"

#include "Core/Stats/LightweightStats.h"
#include "Core/Stats/EditorStats.h"
#include "Core/Async/TaskPool.h"

#include "UObject/UObjectGlobals.h"

#include "Collision/PhysicsInterface.h"
#include "Component/SphereComponent.h"
#include "Collision/PhysicsScene.h"

DECLARE_CYCLE_STAT("Actor Tick", STAT_ActorTick); // Actor 틱 측정
DECLARE_CYCLE_STAT("Update All Transforms", STAT_UpdateAllTransforms); // 각 Transform의 Update 시간 측정
DECLARE_CYCLE_STAT("Gather Render Packets", STAT_GatherRenderPackets);
DECLARE_CYCLE_STAT("Frustum Cull", STAT_FrustumCull);
DECLARE_CYCLE_STAT("Gather Elements", STAT_GatherElements);
DECLARE_CYCLE_STAT("Gather - LOD", STAT_GatherLOD);
DECLARE_CYCLE_STAT("Gather - Submit", STAT_GatherSubmit);

namespace
{
	template <typename T, typename TPredicate>
	T* FindFirstActor(const ULevel* Level, TPredicate Predicate)
	{
		if (!Level)
		{
			return nullptr;
		}

		for (AActor* Actor : Level->GetActors())
		{
			if (T* Typed = Cast<T>(Actor); Typed && Predicate(Typed))
			{
				return Typed;
			}
		}

		return nullptr;
	}

	template <typename T>
	T* FindFirstActor(const ULevel* Level)
	{
		return FindFirstActor<T>(Level, [](const T*) { return true; });
	}
}

UWorld::~UWorld()
{
}

bool UWorld::Init()
{
	// Spawn Actor로 카메라 생성하고 세팅하기
	PersistentLevel = NewObject<ULevel>(this);

	if (!PersistentLevel)
	{
		HTR_LOG(Error, "Failed to create PersistentLevel");
		return false;
	}

	//레벨 연결
	PersistentLevel->SetWorld(this);
	Levels.Add(PersistentLevel);
	CurrentLevel = PersistentLevel;

	return true;
}

void UWorld::Cleanup()
{
	delete PersistentLevel;
}

AActor* UWorld::SpawnActor(UClass* Class, FName InName, const FTransform* Transform)
{
	if (!Class) return nullptr;
	if (!Class->IsChildOf(AActor::StaticClass())) return nullptr;

	// 1. ObjectFactory로 Actor 생성
	AActor* NewActor = NewObject<AActor>(PersistentLevel, Class, InName);

	if (!NewActor)
	{
		HTR_LOG(Error, "SpawnActor : Failed to create Actor");
		return nullptr;
	}

	// 2. Actor에 World/Level 연결
	NewActor->World = this;
	NewActor->Level = PersistentLevel;

	// 3. Transform 적용
	const FTransform SpawnTransform = Transform ? *Transform : FTransform::Identity;

	if (NewActor->GetRootComponent())
	{
		NewActor->GetRootComponent()->SetTransform(SpawnTransform);
	}

	for (UActorComponent* Component : NewActor->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Scene.AddPrimitive(Primitive);
			PhysicsScene.AddPrimitive(Primitive);
		}
		else if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
			Scene.AddFogInfo(Fog->GetUUID(), Fog->GetFogInfo());
		else if (ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
			Scene.AddLight(Light);
		else if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
			Scene.AddDecal(Decal);
	}

	// 4. Level->Actors에 등록
	PersistentLevel->AddActor(NewActor);

	// 5. PlayList에 추가
	NewActorList.Add(NewActor);

	return NewActor;
}

void UWorld::Tick(float DeltaTime)
{
	// 지난 프레임 World Tick 이후(Lua Update 등)에 예약된 액터 파괴
	ProcessPendingDestroys();

	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
		// 프레임 사이에 바뀐 것들을 반영
		Scene.UpdateAllTransforms();
	}

	// 에디터 월드면 더이상 틱을 돌리지 않는다.
	if (WorldType != EWorldType::Editor)
	{
		//인덱스 for 문 사용 : PostInitializeComponent에서 Spawn하게 될 때 범위 for문이 깨진다
		for (int32 i = 0; i < NewActorList.Num(); ++i)
		{
			NewActorList[i]->InitializeComponents();
			NewActorList[i]->PostInitializeComponents();
		}

		// BeginPlay 중에 스폰된 액터는 초기화를 안 거쳤으니 다음 프레임으로 넘긴다
		const int32 StartingNum = NewActorList.Num();
		for (int32 i = 0; i < StartingNum; ++i)
		{
			NewActorList[i]->BeginPlay();
		}
		if (StartingNum != 0)
		{
			NewActorList.RemoveAt(0, StartingNum);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_ActorTick);
			// 모든 Actor를 도는 대신 등록된 Tick 함수만 실행한다.
			TickTaskManager.RunAllTickGroups(DeltaTime);
		}

		//이동이 전부 끝난 후 Overlap 판정을 해야한다.
		//Overlap 판정 후 이벤트 큐를 모으기만 한다.
		PhysicsScene.UpdateOverlaps(*this);

		//Overlap 이벤트들 발송
		PhysicsScene.DispatchPendingEvents();

		// Tick, Overlap 중에 예약된 액터를 그리기 전에 파괴
		ProcessPendingDestroys();

		if (PlayerController)
		{
			PlayerController->UpdateCamera(DeltaTime);
		}

	}	

	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
		// Tick 중에 바뀐 것들을 반영
		Scene.UpdateAllTransforms();
	}
}

void UWorld::ClearWorld()
{
	// BeginPlay 대기 중인 Actor 제거
	NewActorList.Reset();

	// 아래에서 액터를 모두 지우므로 예약 목록에 남으면 dangling이 된다.
	PendingDestroyActors.Reset();

	// 액터를 지우기 전에 렌더 프록시와 틱 등록부터 푼다. ClearActors는 액터를 delete만 하므로,
	// 그대로 두면 지워진 컴포넌트를 가리키는 프록시가 FScene에 남아 다음 프레임에 터진다.
	Scene.RemoveAllPrimitives();
	PhysicsScene.RemoveAllPrimitives();
	Scene.RemoveAllFogInfo();
	Scene.RemoveAllLights();
	Scene.RemoveAllDecals();
	for (ULevel* Level : Levels)
	{
		for (AActor* Actor : Level->Actors)
			if (Actor)
				Actor->RegisterAllActorTickFunctions(false);
		Level->ClearActors();
	}
	
	PlayerController = nullptr;
	bHasPlayerStart = false;

	HTR_LOG(Info, "{} : ", PersistentLevel->GetActorNum());
}

void UWorld::GatherRenderPackets(FRenderQueue& RenderQueue, const FLODViewContext* LODView, const FFrustumPlanes* Frustum, FRenderer* Renderer, const FBillboardView* BillboardView)
{
	// 멤버로 두어 매 프레임 용량을 재사용한다.
	RenderStats.Reset();
	VisibleProxies.Reset();

	{
		SCOPE_CYCLE_COUNTER(STAT_FrustumCull);

		// 컬링 단계에서는 프록시 포인터만 모으고, 컴포넌트 역참조(가시성 확인)는 어차피 컴포넌트를 읽는 Gather로 미룬다.
		const bool bGameView = WorldType != EWorldType::Editor;
		const auto Visit = [&](FPrimitiveSceneProxy* Proxy)
		{
			if (bGameView && Proxy->IsHiddenInGame())
			{
				return;
			}
			VisibleProxies.Add(Proxy);
		};

		if (Frustum)
		{
			Scene.BVH.QueryCull(
				FrustumAllPlanesMask,
				[Frustum](const FBox& Bounds, uint32& Mask)
				{
					return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, *Frustum, Mask));
				},
				Visit);
		}
		else
		{
			Scene.BVH.QueryCull(0, [](const FBox&, uint32&) { return EBVHCullResult::Inside; }, Visit);
		}
	}

	RenderStats.TotalPrimitives = Scene.Proxies.Num();

	// GPU 오클루전: 프러스텀을 통과한 물체를 GPU에서 가림 판정하고 결과를 이번 프레임에 받아 온다.
	// Cull이 켜져 있으면 가려진 물체를 목록에서 빼서 이후 Gather·정렬·드로우를 모두 건너뛴다.
	// 꺼져 있으면(검증 모드) 목록은 그대로 두고 패킷에 판정만 표시한다.
	const uint8* OccludedMask = nullptr;
	if (Renderer && LODView && Renderer->GetGPUOcclusion().GetSettings().bEnabled)
	{
		FGPUOcclusion& Occlusion = Renderer->GetGPUOcclusion();
		if (Occlusion.Run(VisibleProxies.GetData(), VisibleProxies.Num(), *LODView))
		{
			const std::vector<uint8>& Occluded = Occlusion.GetOccluded();
			if (Occlusion.GetSettings().bCull)
			{
				uint32 Kept = 0;
				for (uint32 i = 0; i < VisibleProxies.Num(); ++i)
					if (!Occluded[i])
						VisibleProxies[Kept++] = VisibleProxies[i];
				VisibleProxies.SetNum(Kept);
			}
			else
			{
				OccludedMask = Occluded.data();
			}
		}
	}

	RenderStats.VisiblePrimitives = VisibleProxies.Num();

	constexpr uint32 ExtraSlots = 4096;
	const uint32 VisibleCount = VisibleProxies.Num();
	const uint32 MaxSlots = VisibleCount + ExtraSlots;
	uint8* SlotDest = Renderer ? Renderer->BeginObjectConstants(MaxSlots) : nullptr;
	uint32 NextExtraSlot = VisibleCount;

	// 조각 수 = 스레드 수 × 4. 잘게 나눠야 먼저 끝난 스레드가 남은 조각을 가져가서 부하가 고르게 된다.
	FTaskPool& Pool = FTaskPool::Get();
	const uint32 ChunkCount = FMath::Clamp(VisibleCount, 1u, Pool.GetNumThreads() * 4);

	if (GatherChunks.Num() < ChunkCount)
		GatherChunks.SetNum(ChunkCount);     // 늘릴 때만. 줄이지 않아야 배열 용량이 계속 재사용된다.

	for (uint32 c = 0; c < GatherChunks.Num(); ++c)   // 이번에 안 쓰는 조각의 묶음도 비워 둔다 (Renderer에 넘기지 않도록)
	{
		FGatherChunk& Chunk = GatherChunks[c];
		Chunk.Packets.Reset();               // 용량은 유지, 개수만 0
		Chunk.SlowPathIndices.Reset();
		std::fill(std::begin(Chunk.LODCounts), std::end(Chunk.LODCounts), 0u);
		std::fill(std::begin(Chunk.LODTriangles), std::end(Chunk.LODTriangles), 0ull);
		for (FStaticDrawGroup& Group : Chunk.Groups)
			Group.Items.clear();             // 용량은 유지
		Chunk.StaticDrawCount = 0;
	}

	// Renderer가 있으면 불투명 스태틱 메시는 패킷 대신 조각별 묶음에 작은 항목으로 넣는다.
	// 동일 바인딩의 개별 드로우를 묶어 패킷 복사와 항목별 정렬을 줄인다.
	const bool bStaticGroups = Renderer != nullptr;
	if (Renderer)
		Renderer->ResetStaticDrawGroups();

	{
		SCOPE_CYCLE_COUNTER(STAT_GatherElements);

		LODInputs.Reset();
		if (LODView)
		{
			LODInputs.Reserve(VisibleProxies.Num());
			for (const FPrimitiveSceneProxy* Proxy : VisibleProxies)
				LODInputs.Add({ Proxy->GetLODSphere(), Proxy->GetRenderState() });
			SelectLODs(LODInputs, *LODView, SelectedLODs);
		}
		Pool.ParallelFor(VisibleCount, ChunkCount, [&](uint32 Begin, uint32 End, uint32 ChunkIndex)
			{
				FGatherChunk& Out = GatherChunks[ChunkIndex];      // 이 조각 전용. 다른 스레드는 절대 안 건드림
				// 같은 머티리얼과 버퍼 조합을 공유한다. 인덱스 범위는 각 Item에 보관한다.
				const auto FindGroup = [&Out](UMaterial* Material, FVertexBuffer* const* VertexBuffers, int32 VertexBufferCount, FIndexBuffer* IndexBuffer) -> FStaticDrawGroup&
					{
						if (Out.LastGroup < Out.Groups.size())
						{
							FStaticDrawGroup& Last = Out.Groups[Out.LastGroup];

							if (Last.Material == Material && Last.Geometry.HasSameBuffers(VertexBuffers, VertexBufferCount, IndexBuffer))
							{
								return Last;
							}
						}
						for (uint32 g = 0; g < Out.Groups.size(); ++g)
						{
							FStaticDrawGroup& Group = Out.Groups[g];
							if (Group.Material == Material && Group.Geometry.HasSameBuffers(VertexBuffers, VertexBufferCount, IndexBuffer))
							{
								Out.LastGroup = g;
								return Group;
							}
						}
						Out.LastGroup = static_cast<uint32>(Out.Groups.size());
						FStaticDrawGroup& Group = Out.Groups.emplace_back();
						Group.Material = Material;
						Group.Geometry.SetVertexBuffers(VertexBuffers, VertexBufferCount);
						Group.Geometry.IndexBuffer = IndexBuffer;
						return Group;
					};

				for (uint32 VisibleIndex = Begin; VisibleIndex < End; ++VisibleIndex)
				{
					FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];   // 읽기만
					if (!Proxy->IsVisible()) continue;

					const FMeshRenderState* RenderState = Proxy->GetRenderState();
					if (!RenderState)
					{
						Out.SlowPathIndices.Add(VisibleIndex);     // 컴포넌트를 건드리는 경로는 메인이 나중에
						continue;
					}

					const uint32 LOD = LODView ? SelectedLODs[VisibleIndex] : 0;
					const FCachedMeshLOD& CachedLOD = RenderState->LODs[LOD];

					++Out.LODCounts[LOD];                          // RenderStats 대신 조각 전용 통계

					uint32 Slot = InvalidObjectSlot;
					if (SlotDest)
					{
						// 칸 VisibleIndex는 이 반복만 쓴다 → 스레드끼리 겹치지 않음
						WriteObjectSlot(SlotDest, VisibleIndex, Proxy->GetLocalToWorld(), Proxy->GetNormalMatrix(), Proxy->GetObjectMask());
						Slot = VisibleIndex;
					}

					const bool bOccludedByGpu = OccludedMask && OccludedMask[VisibleIndex];
					for (uint32 i = 0; i < CachedLOD.NumSections; ++i)
					{
						const FCachedMeshSection& Section = Proxy->GetSection(CachedLOD.FirstSection + i);
						Out.LODTriangles[LOD] += Section.IndexCount / 3;

						if (bStaticGroups && Section.Material && Section.Material->BlendState == EBlendState::Opaque)
						{
						FStaticDrawGroup& Group = FindGroup(Section.Material, CachedLOD.VertexBuffers, CachedLOD.VertexBufferCount, CachedLOD.IndexBuffer);
							Group.Items.push_back({ Proxy, Slot, Section.StartIndex, Section.IndexCount, bOccludedByGpu ? 1u : 0u });
							++Out.StaticDrawCount;
							continue;
						}

						FRenderPacket& Packet = Out.Packets.AddDefaulted_GetRef();   // 조각 전용 배열에 추가
						Packet.Proxy = Proxy;
						Packet.Geometry.SetVertexBuffers(CachedLOD.VertexBuffers, CachedLOD.VertexBufferCount);
						Packet.Geometry.IndexBuffer = CachedLOD.IndexBuffer;
						Packet.Material = Section.Material;
						Packet.Geometry.StartIndex = Section.StartIndex;
						Packet.Geometry.IndexCount = Section.IndexCount;
						Packet.Slot = Slot;
						Packet.bOccludedByGpu = bOccludedByGpu;
					}
				}
			});

		// 묶음은 World 메모리 그대로 Renderer에 넘긴다 (복사 없음). 정렬은 Renderer가 묶음 단위로 한다.
		uint32 StaticDraws = 0;
		if (Renderer)
		{
			for (uint32 c = 0; c < ChunkCount; ++c)
			{
				StaticDraws += GatherChunks[c].StaticDrawCount;
				for (const FStaticDrawGroup& Group : GatherChunks[c].Groups)
					if (!Group.Items.empty())
						Renderer->AddStaticDrawGroup(&Group);
			}
		}

		uint32 TotalPackets = 0;
		for (uint32 c = 0; c < ChunkCount; ++c)
			TotalPackets += GatherChunks[c].Packets.Num();

		RenderQueue.Reserve(TotalPackets + 256);             // 느린 경로 몫 약간 여유
		for (uint32 c = 0; c < ChunkCount; ++c)
			if (GatherChunks[c].Packets.Num() > 0)
				RenderQueue.Append(GatherChunks[c].Packets);  // 조각 순서대로 이어 붙이기

		for (uint32 c = 0; c < ChunkCount; ++c)
		{
			for (uint32 VisibleIndex : GatherChunks[c].SlowPathIndices)
			{
				FPrimitiveSceneProxy* Proxy = VisibleProxies[VisibleIndex];
				UPrimitiveComponent* Primitive = Proxy->GetComponent();
				if (!Primitive || !Primitive->IsVisible())
					continue;

				const uint32 FirstNew = RenderQueue.Num();

				// 프록시 캐시가 없는 스태틱 메시는 기존처럼 LOD를 골라 제출하고, 그 외는 컴포넌트에 맡긴다.
				UStaticMeshComponent* StaticMeshComponent = LODView ? Cast<UStaticMeshComponent>(Primitive) : nullptr;
				if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
				{
					const uint32 LOD = SelectedLODs[VisibleIndex];
					StaticMeshComponent->SubmitToRenderQueue(RenderQueue, LOD);
				}
				else if (UBillboardComponent* Billboard = BillboardView ? Cast<UBillboardComponent>(Primitive) : nullptr)
				{
					Billboard->SubmitToRenderQueue(RenderQueue, *BillboardView);
				}
				else
				{
					Primitive->SubmitToRenderQueue(RenderQueue);
				}

				// continue 없이 항상 여기까지 와서 새 패킷에 여유 칸을 배정한다.
				for (uint32 p = FirstNew; p < RenderQueue.Num(); ++p)
				{
					if (!SlotDest || NextExtraSlot >= MaxSlots) break;
					FRenderPacket& Packet = RenderQueue[p];
					if (Packet.Proxy)
					{
						WriteObjectSlot(SlotDest, NextExtraSlot, Packet.Proxy->GetLocalToWorld(), Packet.Proxy->GetNormalMatrix(), Packet.Proxy->GetObjectMask());
					}
					else
					{
						// 프록시 없는 패킷(빌보드 등)은 캐시가 없으므로 여기서 계산한다.
						const FMatrix& Model = Packet.Model ? *Packet.Model : FMatrix::Identity;
						WriteObjectSlot(SlotDest, NextExtraSlot, Model, MakeNormalMatrix(Model), 0u);
					}
					Packet.Slot = NextExtraSlot++;
				}
			}
		}

		for (uint32 c = 0; c < ChunkCount; ++c)
			for (uint32 L = 0; L < 4; ++L)
			{
				RenderStats.LODCounts[L] += GatherChunks[c].LODCounts[L];
				RenderStats.LODTriangles[L] += GatherChunks[c].LODTriangles[L];
			}

		if (SlotDest) Renderer->EndObjectConstants();
		RenderStats.DrawCalls = RenderQueue.Num() + StaticDraws;
		for (uint64 T : RenderStats.LODTriangles) RenderStats.Triangles += T;
	}
}

//void UWorld::GatherRenderPackets(TArray<FRenderPacket>& RenderArray, const FLODViewContext* LODView, const FFrustumPlanes* Frustum)
//{
//	for (TObjectIterator<UPrimitiveComponent> Itr; Itr; ++Itr)
//	{
//		if (!*Itr || !Itr->IsVisible())
//			continue;
//
//		if (Frustum)
//		{
//			const FBox Box = Itr->CalcBounds();
//			const FAABB Bounds{
//				(Box.Min + Box.Max) * 0.5f,
//				(Box.Max - Box.Min) * 0.5f
//			};
//			if (!IsAABBInFrustum(Bounds, *Frustum))
//				continue;
//		}
//
//		if (LODView)
//		{
//			if (auto* Component = Cast<UStaticMeshComponent>(*Itr))
//			{
//				if (UStaticMesh* Mesh =
//					Component->GetStaticMesh())
//				{
//					const uint32 LOD = SelectStaticMeshLOD(
//						*Mesh,
//						Component->GetWorldMatrix(),
//						*LODView);
//
//					Component->SubmitToRenderQueue(RenderQueue, LOD);
//					continue;
//				}
//			}
//		}
//
//		Itr->SubmitToRenderQueue(RenderQueue);
//	}
//}

int32 UWorld::GetActorNum()
{
	return PersistentLevel->GetActorNum();
}

bool UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor)
		return false;

	ULevel* Level = Actor->GetLevel();

	if (!Level)
		return false;

	if (Actor->HasActorBegunPlay())
	{
		Actor->EndPlay();
	}

	// 예약된 채로 즉시 파괴되면(일시정지 PIE에서 에디터 삭제 등) 예약 목록에서 비운다.
	// ProcessPendingDestroys가 인덱스로 돌고 있을 수 있으니 지우지 않고 nullptr로 둔다.
	const int32 PendingIndex = PendingDestroyActors.Find(Actor);
	if (PendingIndex != -1)
	{
		PendingDestroyActors[PendingIndex] = nullptr;
	}

	// 1. BeginPlay 대기열에서 제거
	for (int32 i = 0; i < NewActorList.Num(); i++)
	{
		AActor* PendingActor = NewActorList[i];
		if (PendingActor == Actor)
		{
			NewActorList.RemoveAtSwap(i);
			i--;
		}
	}

	// 2. Level의 Actors에서 제거
	for (int32 i = Level->Actors.Num() - 1; i >= 0; --i)
	{
		if (Level->Actors[i] == Actor)
		{
			Level->Actors.RemoveAt(i, 1);
			break;
		}
	}

	FString ActorName = Actor->GetName();
	uint32 ActorUUID = Actor->GetUUID();

	// 3. 프록시 제거
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Scene.RemovePrimitive(Primitive);
			PhysicsScene.RemovePrimitive(Primitive);
		}
		else if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
		{
			Scene.RemoveFogInfo(Fog->GetUUID());
		}
		else if (ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
		{
			Scene.RemoveLight(Light);
		}
		else if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
		{
			Scene.RemoveDecal(Decal);
		}
	}

	Actor->RegisterAllActorTickFunctions(false);

	if (Actor == PlayerController)
	{
		PlayerController = nullptr;
	}

	if (PlayerController)
	{
		PlayerController->NotifyActorDestroyed(Actor);
	}

	// 4. Actor 삭제
	// Actor destruction notification
	OnActorDestroyed.Broadcast(Actor);
	delete Actor;

	HTR_LOG(Info, "Destroy Actor : {} UUID {}", ActorName, ActorUUID);

	return true;
}

bool UWorld::RequestDestroyActor(AActor* Actor)
{
	if (!Actor) return false;

	// 중복 요청은 false. Lua에서 첫 요청인지 구분하는 데 쓴다.
	if (PendingDestroyActors.Find(Actor) != -1)
	{
		return false;
	}

	PendingDestroyActors.Add(Actor);
	return true;
}

// 다른 World의 객체를 제외하고 Component 교차 중 최근접 결과를 선택한다.
bool UWorld::LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
	FBillboardTraceTransform ResolveBillboard, const void* ViewContext)
{
	SCOPE_CYCLE_COUNTER_ALWAYS(EditorStats::STAT_PickingTime_Name);
	OutHit = FHitResult();
	float NearestT = std::numeric_limits<float>::max();

	const auto TraceComponent = [&](FPrimitiveSceneProxy* Proxy, float& InOutNearestT) {
		UPrimitiveComponent* Component = Proxy ? Proxy->GetComponent() : nullptr;

		if (!Component || !Component->IsVisible())
		{
			return false;
		}

		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			if (!ResolveBillboard)
			{
				return false;
			}

			const FMatrix BillboardToWorld = ResolveBillboard(*Billboard, ViewContext);

			const FRay LocalRay = ToLocalRay(WorldRay, BillboardToWorld);

			float T = InOutNearestT;
			if (!Billboard->LineTraceComponentLocal(LocalRay, T))
			{
				return false;
			}

			OutHit.HitComponent = Billboard;
			OutHit.Distance = T;
			OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
			InOutNearestT = T;
			return true;
		}

		const FMatrix& WorldToLocal = Proxy->GetWorldToLocal();
		const FRay LocalRay{
			.Origin = WorldToLocal.TransformPosition(WorldRay.Origin),
			.Direction = WorldToLocal.TransformVector(WorldRay.Direction)
		};

		float T = InOutNearestT;
		if (!Component->LineTraceComponentLocal(LocalRay, T))
		{
			return false;
		}

		OutHit.HitComponent = Component;
		OutHit.Distance = T;
		OutHit.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;
		InOutNearestT = T;
		return true;
	};

	const FPreparedRay PreparedRay(WorldRay);

	Scene.BVH.TraceClosest(
		[&](const FBox& Bounds, float& OutEnterT) { return RayIntersectsAABB(PreparedRay, Bounds.Min, Bounds.Max, OutEnterT); },
		[&](FPrimitiveSceneProxy* Proxy, float& OutNearestT) { return TraceComponent(Proxy, OutNearestT); },
		NearestT);

	return OutHit.HitComponent != nullptr;
}

void UWorld::BeginPlay()
{
	SpawnPlayerController();
	RestartPlayer();
}

void UWorld::EndPlay()
{
	for (ULevel* Level : Levels)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->HasActorBegunPlay())
			{
				Actor->EndPlay();
			}
		}
	}
}

UWorld* UWorld::CreateWorld(const EWorldType InWorldType, bool bInformEngineOfWorld, FName WorldName)
{
	UWorld* NewWorld = NewObject<UWorld>(nullptr, WorldName);
	NewWorld->SetFlags(EObjectFlags::RF_Transactional);
	NewWorld->WorldType = InWorldType;
	NewWorld->Init();

	return NewWorld;
}

void UWorld::DestroyWorld(UWorld* InWorld)
{
	InWorld->ClearWorld();
	InWorld->Cleanup();
	delete InWorld;
}

UWorld* UWorld::GetDuplicatedWorldForPIE(UWorld * InWorld)
{
	if (!InWorld)
	{
		return nullptr;
	}

	UWorld* PIEWorld = CreateWorld(EWorldType::PIE, false);

	FObjectDuplicationParameters Params(InWorld, nullptr);
	Params.DuplicationSeed.Add(InWorld, PIEWorld);
	Params.DuplicationSeed.Add(InWorld->GetPersistentLevel(), PIEWorld->GetPersistentLevel());
	Params.PortFlags = EPropertyPortFlags::PPF_DuplicateForPIE;

	StaticDuplicateObjectEx(Params);
	return PIEWorld;
}

void UWorld::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// 아직 필요 없는애들
	//TArray<UObject*> ObjectsToFixReferences;
	//TMap<UObject*, UObject*> ReplacementMap;

	if (!bDuplicateForPIE)
	{
		assert(PersistentLevel);

		// Update the persistent level's owning world. This is needed for some initialization
		if (!PersistentLevel->OwningWorld)
		{
			PersistentLevel->OwningWorld = this;
		}
	}

	for (AActor* Actor : PersistentLevel->GetActors())
	{
		if (!Actor)
			continue;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
				SceneComp->MarkTransformDirty();        // 부착/상대 Transform이 바뀌었으니 다시 계산

			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Scene.AddPrimitive(Primitive);
				PhysicsScene.AddPrimitive(Primitive);
			}
			else if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
				Scene.AddFogInfo(Fog->GetUUID(), Fog->GetFogInfo());
			else if (ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
				Scene.AddLight(Light);
			else if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
				Scene.AddDecal(Decal);
		}

		NewActorList.Add(Actor);   // Tick 등록은 BeginPlay에서 함
	}

	Scene.BuildBVH();
}

void UWorld::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // "Properties"

	FArchive& Ar = Record.GetUnderlyingArchive();
	PersistentLevel->Serialize(Record.EnterRecord("PersistentLevel"));
}

void UWorld::SpawnPlayerController()
{
	PlayerController = SpawnActor<APlayerController>(FName("PlayerController"));
	PlayerController->SetFlags(EObjectFlags::RF_Transient);
}

// 우선순위: Auto Possess Pawn > PlayerStart > 원점
void UWorld::RestartPlayer()
{
	if (APawn* Pawn = FindFirstActor<APawn>(PersistentLevel,
		[](const APawn* P) { return P->IsAutoPossessPlayer(); }))
	{
		bHasPlayerStart = true;
		PlayerController->Possess(Pawn);
		return;
	}

	APlayerStart* Start = FindFirstActor<APlayerStart>(PersistentLevel);
	bHasPlayerStart = Start != nullptr;

	const FVector StartLocation = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	FRotator StartRotation = Start ? Start->GetActorRotation() : FRotator::Identity;
	StartRotation.Roll = 0.0f;

	const FTransform SpawnTransform(StartLocation, FVector(1.f), FRotator(0.0f, StartRotation.Yaw, 0.0f));
	ADefaultPawn* Pawn = SpawnActor<ADefaultPawn>(NAME_None, &SpawnTransform);
	Pawn->SetFlags(EObjectFlags::RF_Transient);

	PlayerController->Possess(Pawn);
	PlayerController->SetControlRotation(StartRotation);
}

void UWorld::ProcessPendingDestroys()
{
	// 파괴 중 또 Destroy가 요청되며 늘어날 수 있다.
	// range-for 금지. 인덱스로 끝까지 돈다. Num()을 매번 다시 읽는다
	for (int32 i = 0; i < PendingDestroyActors.Num(); ++i)
	{
		DestroyActor(PendingDestroyActors[i]);
	}

	PendingDestroyActors.Reset();
}

void UWorld::SetGlobalTimeDilation(float NewDilation)
{
	GlobalTimeDilation = FMath::Clamp(NewDilation, 0.001f, 10.0f);
}

float UWorld::GetGlobalTimeDilation() const
{
	return GlobalTimeDilation;
}
