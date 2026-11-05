#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Transform.h"
#include "Render/Renderer.h"
#include "Math/Frustum.h"
#include "Engine/Scene.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Engine/EngineTypes.h"
#include "Collision/PhysicsScene.h"
#include "Core/Delegate.h"

class ULevel;
class UBillboardComponent;
class APlayerController;

struct FLODViewContext;

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	void Reset() { *this = FRenderStats(); }
};

class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

public:
	UWorld() = default;
	virtual ~UWorld();

	bool Init();
	void Cleanup();

	AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FTransform* Transform = nullptr);

	template <class T>
	T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), InName, Transform));
	}

	void Tick(float DeltaTime);

	void ClearWorld();

	void GatherRenderPackets(FRenderQueue& RenderArray, const FLODViewContext* LODView = nullptr, const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr, const FBillboardView* BillboardView = nullptr);

	bool HasPlayerStart() const { return bHasPlayerStart; }
	APlayerController* GetPlayerController() const { return PlayerController; }

	// Level
	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	void SetPersistentLevel(ULevel* InLevel) { PersistentLevel = InLevel; }

	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }

	int32 GetActorNum();

	// 즉시 파괴. 순회 루프 밖(에디터 조작 등)에서만 부른다.
	bool DestroyActor(AActor* Actor);
	// 항상 예약하고 World Tick에서 파괴한다. 게임 코드, Lua는 이쪽을 쓴다.
	// 이미 예약된 액터면 false를 반환한다.
	bool RequestDestroyActor(AActor* Actor);

	// 액터 메모리 해제 이전의 외부 참조 정리 통지
	TMulticastDelegate<AActor*> OnActorDestroyed;

	// View별 Billboard 행렬 공급자는 이 동기 호출 동안만 사용하며 저장하지 않는다.
	using FBillboardTraceTransform = FMatrix(*)(const UBillboardComponent&, const void*);
	// 현재 World의 Component에 Ray를 전달하고 가장 가까운 유효 교차를 반환한다.
	bool LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
		FBillboardTraceTransform ResolveBillboard = nullptr, const void* ViewContext = nullptr);

	void BeginPlay();
	void EndPlay();

	const FScene& GetScene() const { return Scene; }
	FScene& GetScene() { return Scene; }
	FPhysicsScene& GetPhysicsScene() { return PhysicsScene; }
	FTickTaskManager& GetTickTaskManager() { return TickTaskManager; }

	const FRenderStats& GetRenderStats() const { return RenderStats; }

	void SetWorldType(EWorldType InType) { WorldType = InType; }
	EWorldType GetWorldType() const { return WorldType; }

	static UWorld* CreateWorld(const EWorldType InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None); /*, UPackage* InWorldPackage = NULL, bool bAddToRoot = true, ERHIFeatureLevel::Type InFeatureLevel = ERHIFeatureLevel::Num, const InitializationValues* InIVS = nullptr, bool bInSkipInitWorld = false);*/
	static void DestroyWorld(UWorld* InWorld);

	bool IsPlayInEditor() const { return WorldType == EWorldType::PIE; }

	static UWorld* GetDuplicatedWorldForPIE(UWorld* InWorld);

	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void SetGlobalTimeDilation(float NewDilation);
	float GetGlobalTimeDilation() const;

private:
	//예약되어 있는 액터들 파괴
	void ProcessPendingDestroys();

	void SpawnPlayerController();
	void RestartPlayer();

private:
	EWorldType WorldType = EWorldType::None;

	struct alignas(64) FGatherChunk
	{
		TArray<FRenderPacket> Packets;              // 스태틱 묶음에 못 들어가는 것 (반투명 섹션, Renderer 없는 호출)
		TArray<uint32> SlowPathIndices;
		std::vector<FStaticDrawGroup> Groups;       // 불투명 스태틱 메시: (머티리얼, 메시, LOD)별 묶음. 항목만 매 프레임 비운다
		uint32 LastGroup = 0;                       // 바로 전 물체가 들어간 묶음 (연속한 물체는 대개 같은 묶음)
		uint32 StaticDrawCount = 0;
		uint32 LODCounts[4] = {};
		uint64 LODTriangles[4] = {};
	};

	TArray<FGatherChunk> GatherChunks;

	// 등록된 Tick 함수만 실행한다. Actor보다 먼저 사라져도 남은 함수와의 연결을 스스로 끊는다.
	FTickTaskManager TickTaskManager;
	float GlobalTimeDilation = 1.0f;

	TArray<AActor*> NewActorList;	

	bool bHasPlayerStart = false;
	APlayerController* PlayerController = nullptr;

	ULevel* PersistentLevel = nullptr;
	ULevel* CurrentLevel = nullptr;
	TArray<ULevel*> Levels;

	FScene Scene;
	FPhysicsScene PhysicsScene;

	// GatherRenderPackets가 매 프레임 채우는 컬링 결과. 용량을 재사용한다.
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;

	FRenderStats RenderStats;

	//지연 파괴 목록
	TArray<AActor*> PendingDestroyActors;
};
