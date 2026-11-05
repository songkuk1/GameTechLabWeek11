#pragma once

#include "SceneComponent.h"
#include "../Render/Shader.h"
#include "../Render/Mesh.h"
#include "Render/RenderPacket.h"
#include "Render/GeometryGenerator.h"
#include "Collision/HitResult.h"
#include "Collision/CollisionShape.h"
#include "Core/Delegate.h"

class FPrimitiveSceneProxy;

enum class EPrimitiveType
{
	Sphere,
	Cube,
	Cone,
	Plane,

	Particle,
	ParticleQuad
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	// Material은 여기서 등록하지 않는다. 컴포넌트마다 머티리얼을 다루는 방식이 달라서
	// (빌보드는 단일 머티리얼, 스태틱 메시는 슬롯별 덮어쓰기) 각자 등록한다.
	REFLECT_START(ClassName)
		PROPERTY(bVisible)
		PROPERTY(bCastShadow)
		PROPERTY(bReceiveShadow)
		PROPERTY(bGenerateOverlapEvents)
		PROPERTY(bBlockComponet)
		PROPERTY(ShowBeginOverlapLog)
		PROPERTY(ShowEndOverlapLog)
		PROPERTY(ShowHitLog)
		PROPERTY(bReceivesDecals)
	REFLECT_END()

public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent();

	virtual void BeginPlay() override;

	// Todo: subuv
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue);

	virtual const FModelMeshData* GetMeshData() const { return nullptr; }

	// Outline처럼 컴포넌트를 통째로 한 번 더 그릴 때 쓰는 GPU 메시.
	// 메시가 없거나(텍스트) View에 따라 형상이 정해지는 컴포넌트(빌보드·파티클)는
	// 컴포넌트 월드 행렬만으로 같은 그림을 못 만들므로 nullptr을 돌려준다.
	virtual UStaticMesh* GetRenderMesh() const { return nullptr; }

	virtual int32 GetNumMaterials() const { return 0; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) {}

	virtual void PostEditChangeProperty(const FProperty& Property) override;
	// 이전 Receiver 저장 이름의 읽기 호환
	using Super::Serialize;
	void Serialize(json& Handle, bool bIsLoading) override;
	void Serialize(FStructuredArchive::FRecord Record) override;
	//
	// FShader* GetShader() const { return Shader.get(); };

	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible)
	{
		bVisible = bInVisible;
		MarkRenderStateDirty();
	}
	bool IsCastShadow() const { return bCastShadow; }
	void SetCastShadow(bool bInCastShadow)
	{
		bCastShadow = bInCastShadow;
		MarkRenderStateDirty();
	}
	bool IsReceiveShadow() const { return bReceiveShadow; }
	void SetReceiveShadow(bool bInReceiveShadow)
	{
		bReceiveShadow = bInReceiveShadow;
		MarkRenderStateDirty();
	}
	bool ReceivesDecals() const
	{
		return bReceivesDecals;
	}
	void SetReceivesDecals(bool bInReceivesDecals)
	{
		bReceivesDecals = bInReceivesDecals;
		MarkRenderStateDirty();
	}
	virtual void SetHiddenInGame(bool bInHidden) override
	{
		Super::SetHiddenInGame(bInHidden);
		MarkRenderStateDirty();
	}

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit);
	virtual bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT);
	virtual FBox CalcLocalBounds() const override
	{
		const FModelMeshData* Data = GetMeshData();
		return Data ? Data->AABB : Super::CalcLocalBounds();
	}

	FPrimitiveSceneProxy* GetSceneProxy() const { return SceneProxy; }

	void MarkRenderStateDirty();

	virtual void OnTransformDirty() override;

	virtual bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit = nullptr) override;

	//콜리전
	virtual FCollisionShape GetCollisionShape() const;

	bool GetGenerateOverlapEvents()const { return bGenerateOverlapEvents; }
	bool IsBlockComponent()const { return bBlockComponet; }
	void SetBlockComponent(bool bInBlockComponent) { bBlockComponet = bInBlockComponent; }

	const TArray<UPrimitiveComponent*>& GetOverlapComps() const;
	TArray<UPrimitiveComponent*> TakeOverlapComps();
	void SetOverlapComps(const TArray<UPrimitiveComponent*>& InOverlapComps);

	bool IsOverlappingComponent(const UPrimitiveComponent* Other) const;
	bool IsOverlappingActor(const AActor* Other) const;

	// FPhysScene이 호출 : delegate 호출
	void DispatchBeginOverlap(UPrimitiveComponent* Other);
	void DispatchEndOverlap(UPrimitiveComponent* Other);

	// Hit 이벤트 이름 고민
	void DispatchBlockingHit(UPrimitiveComponent* Ohter, FHitResult& HitResult, bool bSelfMoved);

	//해당 콜리전 컴포넌트를 목록에서 제거
	bool RemoveOverlapComponent(UPrimitiveComponent* RemoveComp);
	void RemoveOverlapCompAndDispatchEndOverlap(UPrimitiveComponent* RemoveComp);

	void SetNotifyScriptCollision(bool bNotify);
	bool GetNotifyScriptCollision() const;

	virtual FPrimitiveSceneProxy* CreateSceneProxy();

protected:
	bool TraceMesh(const FRay& WorldRay, const FModelMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult);
	bool TraceMeshLocal(const FRay& LocalRay, const FModelMeshData& Mesh, float& OutT);
	bool bVisible = true;
	bool bCastShadow = true;
	bool bReceiveShadow = true;
	bool bReceivesDecals = true;
	bool bGenerateOverlapEvents = true;
	bool bBlockComponet = false;

private:
	friend class AActor;
	friend class FScene;
	friend class FPhysicsScene;
	FPrimitiveSceneProxy* SceneProxy = nullptr;

	// 등록된 충돌 목록과 그 안의 위치. FPhysicsScene만 바꾼다
	FPhysicsScene* RegisteredPhysicsScene = nullptr;
	int32 PhysicsIndex = -1;

	//Overlap된 컴포넌트들
	TArray<UPrimitiveComponent*> OverlappingComponents;

	TMulticastDelegate<UPrimitiveComponent*> OnBeginOverlap;
	TMulticastDelegate<UPrimitiveComponent*> OnEndOverlap;
	TMulticastDelegate<UPrimitiveComponent*, FHitResult&, bool> OnHit;

	bool ShowBeginOverlapLog = false;
	bool ShowEndOverlapLog = false;
	bool ShowHitLog = false;
	bool bNotifyScriptCollision = true;
};
