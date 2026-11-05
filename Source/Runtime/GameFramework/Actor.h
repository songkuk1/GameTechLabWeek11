#pragma once

#include "Camera/CameraTypes.h"
#include "UObject/Object.h"
#include "Component/PrimitiveComponent.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "Containers/Set.h"
#include "Engine/EngineBaseTypes.h"

class UWorld;
class ULevel;

class AActor : public UObject
{
	DECLARE_CLASS(AActor, UObject)
	REFLECT_START(ClassName)
		REFLECT_END()
public:
	AActor();
	virtual ~AActor();

	virtual void PostLoad() override;
	virtual void InitializeComponents();
	virtual void PostInitializeComponents();
	virtual void BeginPlay(); // xx World->AddPrimitive 책임이동 필요
	// 액터 자신의 로직. 컴포넌트는 각자의 PrimaryComponentTick으로 따로 실행된다.
	virtual void Tick(float DeltaTime) {}
	virtual void EndPlay();
	bool HasActorBegunPlay() const { return bActorHasBegunPlay; }
	// FActorTickFunction이 호출하는 진입점
	void TickActor(float DeltaTime) { Tick(DeltaTime); }

	UWorld* GetWorld() const override { return World; }
	ULevel* GetLevel() const { return Level; }

	const TArray<UActorComponent*>& GetComponents() const { return Components; }
	USceneComponent* GetRootComponent() const { return RootComponent; }
	void SetRootComponent(USceneComponent* SceneComponent) { RootComponent = SceneComponent; }
	// 소유한 컴포넌트를 새 루트로 바꾼다. 월드 위치는 유지하고, 옛 루트는 자식들과 함께 새 루트 아래로 붙는다.
	bool ReplaceRootComponent(USceneComponent* NewRoot);

	void RemoveOwnedComponent(UActorComponent* Component);
	
	FVector GetActorLocation() const;
	void SetActorLocation(const FVector& Location);
	FRotator GetActorRotation() const;
	void SetActorRotation(const FRotator& Rotation);
	FVector GetActorScale3D() const;
	FQuat GetActorQuat() const;           //xx 타입명변경
	void SetActorTransform(const FTransform& Transform);
	FTransform GetActorTransform() const;
	bool TryGetActorBounds(FBox& OutBounds) const;
	virtual FMinimalViewInfo CalcCamera(float DeltaTime);

	bool Destroy();
	
	//xx삭제 예정
	friend class UWorld;

	UActorComponent* AddComponent(UClass* ComponentClass, FName Name);
	void DestroyComponent(UActorComponent* Component);

	// bCanEverTick이 켜진 액터·컴포넌트의 Tick 함수만 World의 FTickTaskManager에 등록하거나 해제한다.
	void RegisterAllActorTickFunctions(bool bRegister);

	// UE와 같이 기본값은 bCanEverTick = false. 생성자에서 Target = this
	FActorTickFunction PrimaryActorTick;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	UActorComponent* FindComponentByName(FName Name) const;
	UActorComponent* FindComponentByClass(const UClass* Name) const;

	virtual bool IsOverlappingActor(const AActor* Other) const;

protected:
	virtual void OnDefaultSubobjectCreated(UObject* Subobject) override;

	//TSet<TObjectPtr<UActorComponent>> OwnedComponents;
	TArray<UActorComponent*> Components;
	USceneComponent* RootComponent = nullptr;

	UWorld* World = nullptr;
	ULevel* Level = nullptr;
private:
	bool bActorHasBegunPlay = false;

};