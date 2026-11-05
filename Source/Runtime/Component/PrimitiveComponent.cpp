#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchiveSlots.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";
			break;
		case EPrimitiveType::Cube:
			return "Cube";
			break;
		case EPrimitiveType::Cone:
			return "Cone";
			break;
		case EPrimitiveType::Plane:
			return "Plane";
			break;
		default:
			return "";
			break;
		}
	}
}

UPrimitiveComponent::UPrimitiveComponent()
{
	//SetMaterial(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	// RemovePrimitive를 거치지 않고 지워져도 충돌 목록에 dangling이 남지 않게
	if (RegisteredPhysicsScene)
		RegisteredPhysicsScene->UnregisterPrimitive(this);
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UPrimitiveComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
	//if (Mesh && Material)
	//{
	//	FRenderPacket rp;
	//	rp.mesh = Mesh;
	//	rp.material = Material;
	//	rp.model = GetWorldMatrix();

	//	// Todo: subuv
	//	//rp.bSubUV = false;

	//	RenderQueue.Enqueue(rp);
	//}
}

bool UPrimitiveComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	const FModelMeshData* Mesh = GetMeshData();
	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkRenderStateDirty(SceneProxy);
}

bool UPrimitiveComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	const FModelMeshData* Mesh = GetMeshData();
	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
}

void UPrimitiveComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	MarkRenderStateDirty();
}

// 이전 JSON Receiver 이름의 현재 설정 이관
void UPrimitiveComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);
	if (bIsLoading && !Handle.contains("bReceivesDecals") && Handle.contains("bReceiveDecals"))
	{
		SetReceivesDecals(Handle["bReceiveDecals"].get<bool>());
	}
}

// 이전 구조화 Receiver 이름의 현재 설정 이관
void UPrimitiveComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	FArchive& Ar = Record.GetUnderlyingArchive();
	if (!Ar.IsLoading() || Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		return;
	}

	FStructuredArchive::FRecord Properties = Record.EnterRecord("Properties");
	bool bReceiveDecals = bReceivesDecals;
	if (TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField("bReceivesDecals", false))
	{
		Slot.GetValue() << bReceiveDecals;
	}
	else if (TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField("bReceiveDecals", false))
	{
		Slot.GetValue() << bReceiveDecals;
	}
	SetReceivesDecals(bReceiveDecals);
}


void UPrimitiveComponent::OnTransformDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkDirty(SceneProxy);
}

bool UPrimitiveComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	if (!bSweep)
	{
		SetWorldLocationAndRotation(Transform.Location + Delta, NewRotation, bSweep);
	}
	else
	{
		//형상이 None이라면 일반 이동과 같이 처리
		if (GetCollisionShape().ShapeType == ECollisionShape::None || bBlockComponet == false || Delta == FVector::ZeroVector) 
		{
			SetWorldLocationAndRotation(Transform.Location + Delta, NewRotation, bSweep);
			return true;
		}

		if (Hit)
		{
			Hit->Init();			
		}

		//같은 월드의 다른 컴포넌트들과 검사
		FHitResult Result;
		AActor* Owner = GetOwner();
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		if (!World) return false;

		FPhysicsScene& PhysicsScene = World->GetPhysicsScene();
		bool bHit = false;
		bHit = World->GetPhysicsScene().ComponentSweepSingle(this, Delta, NewRotation, Result);
		
		//충돌이 없다면 그냥 이동
		if (!bHit)
		{
			SetWorldLocationAndRotation(Transform.Location + Delta, NewRotation, bSweep);
			return true;
		}

		//Hit 처리되었다
		//t 지점보다 조금 덜 간다
		float Dist = Delta.Size();
		float MoveDist = Dist * Result.Time;
		float PullBack = FMath::Min(0.01f, MoveDist);
		FVector	ActualMove = Delta / Dist * (MoveDist - PullBack);
		SetWorldLocationAndRotation(Transform.Location + ActualMove, NewRotation, bSweep);

		if (Hit)
		{
			*Hit = Result;
		}

		PhysicsScene.QueueHit(FPendingHitEvent{this, Result.HitComponent, Result, true});

		Result.Normal *= -1.f;
		Result.ImpactNormal *= -1.f;
		UPrimitiveComponent* TempComp = Result.HitComponent;
		Result.HitComponent = this;
		PhysicsScene.QueueHit(FPendingHitEvent{ TempComp, this, Result, false});
		
		return true;
	}

	return true;
}

FCollisionShape UPrimitiveComponent::GetCollisionShape() const
{
	return FCollisionShape();
}


const TArray<UPrimitiveComponent*>& UPrimitiveComponent::GetOverlapComps() const
{
	return OverlappingComponents;
}

TArray<UPrimitiveComponent*> UPrimitiveComponent::TakeOverlapComps()
{
	TArray<UPrimitiveComponent*> ReturnArray = std::move(OverlappingComponents);
	OverlappingComponents.Reset();
	return ReturnArray;
}

void UPrimitiveComponent::SetOverlapComps(const TArray<UPrimitiveComponent*>& InOverlapComps)
{
	OverlappingComponents = InOverlapComps;
}

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* Other) const
{
	if (!Other) return false;

	// 겹침 목록은 PhysicsScene이 매 프레임 갱신한다 (Begin을 보낸 상대만 들어 있다)
	for (const UPrimitiveComponent* OverlapComp : OverlappingComponents)
	{
		if (OverlapComp == Other) return true;
	}
	return false;
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
	if (!Other) return false;

	// 상대 액터의 컴포넌트 중 하나라도 나와 겹쳐 있으면 그 액터와 겹친 것
	for (const UPrimitiveComponent* OverlapComp : OverlappingComponents)
	{
		if (OverlapComp && OverlapComp->GetOwner() == Other) return true;
	}
	return false;
}

void UPrimitiveComponent::DispatchBeginOverlap(UPrimitiveComponent* Other)
{
	if (ShowBeginOverlapLog)
	{
		HTR_LOG(Warning, "{} : Begin Overlap. Other : {}", GetName(), Other->GetName());
	}

	OnBeginOverlap.Broadcast(Other);
}

void UPrimitiveComponent::DispatchEndOverlap(UPrimitiveComponent* Other)
{
	if (ShowEndOverlapLog)
	{
		HTR_LOG(Warning, "{} : End Overlap. Other : {}", GetName(), Other->GetName());
	}

	OnEndOverlap.Broadcast(Other);
}

void UPrimitiveComponent::DispatchBlockingHit(UPrimitiveComponent* Other, FHitResult& HitResult, bool bSelfMoved)
{
	if (ShowHitLog)
	{
		HTR_LOG(Warning, "Hit Comp {}. Other Comp {}", GetName(), Other->GetName());
	}

	OnHit.Broadcast(Other, HitResult, bSelfMoved);
}

bool UPrimitiveComponent::RemoveOverlapComponent(UPrimitiveComponent* RemoveComp)
{
	if (!RemoveComp) return false;

	int32 Index = OverlappingComponents.Find(RemoveComp);
	if (Index == -1) return false;

	//오버랩 목록에서 제거
	OverlappingComponents.RemoveAtSwap(Index);
	return true;
}

void UPrimitiveComponent::RemoveOverlapCompAndDispatchEndOverlap(UPrimitiveComponent* RemoveComp)
{
	if (RemoveOverlapComponent(RemoveComp))
	{
		DispatchEndOverlap(RemoveComp);
	}
}

bool UPrimitiveComponent::TraceMesh(const FRay& WorldRay, const FModelMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult)
{
	FBox MeshBox = Mesh.AABB.GetWorldAABB(WorldMatrix);
	float BoxT;
	if (!RayIntersectsAABB(WorldRay, MeshBox.Min, MeshBox.Max, BoxT))
	{
		return false;
	}

	FRay LocalRay = ToLocalRay(WorldRay, WorldMatrix);
	float T = FLT_MAX;

	if (!RayIntersectsMesh(LocalRay, Mesh, T)) return false;

	OutResult.HitComponent = this;
	OutResult.Distance = T;
	OutResult.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;

	return true;
}

bool UPrimitiveComponent::TraceMeshLocal(const FRay& LocalRay, const FModelMeshData& Mesh, float& OutT)
{
	return RayIntersectsMesh(LocalRay, Mesh, OutT);
}

void UPrimitiveComponent::SetNotifyScriptCollision(bool bNotify)
{
	bNotifyScriptCollision = bNotify;
}

bool UPrimitiveComponent::GetNotifyScriptCollision() const
{
	return bNotifyScriptCollision;
}

FPrimitiveSceneProxy* UPrimitiveComponent::CreateSceneProxy()
{
	return new FPrimitiveSceneProxy(this);
}
