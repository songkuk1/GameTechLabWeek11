#include "EnginePCH.h"
#include "Actor.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Component/SceneComponent.h"
#include "Component/ParticleSubUVComponent.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "Component/LightComponentBase.h"
#include "Core/EngineLog.h"
#include "UObject/Object.h"
#include "Component/ScriptComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/MovementComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Actor/DecalActor.h"

enum class EAttachmentRule;


AActor::AActor()
{
	PrimaryActorTick.Target = this;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("DefaultSceneRoot"));
}

AActor::~AActor()
{
	TArray<UActorComponent*> ToDelete = Components;
	Components.Reset();
	RootComponent = nullptr;

	for (UActorComponent* Component : ToDelete)
	{
		delete Component;
	}
}

void AActor::PostLoad()
{
	Super::PostLoad();
	
	for (UActorComponent* Component : Components)
	{
		Component->PostLoad();
	}
}

void AActor::InitializeComponents()
{
	for (UActorComponent* Component : Components)
	{
		Component->InitializeComponent();
	}
}

void AActor::PostInitializeComponents()
{
	
}

void AActor::BeginPlay()
{
	for (UActorComponent* Component : Components)
	{
		Component->BeginPlay();
	}

	RegisterAllActorTickFunctions(true);
	bActorHasBegunPlay = true;

	for (auto* Component : Components)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive)
		{
			continue;
		}

		for (auto* Component_2 : Components)
		{
			UScriptComponent* ScriptComp = Cast<UScriptComponent>(Component_2);
			if (!ScriptComp || !Primitive->GetNotifyScriptCollision())
			{
				continue;
			}

			Primitive->OnBeginOverlap.AddRaw(ScriptComp, &UScriptComponent::HanldeOnCollisionEnter);
			Primitive->OnEndOverlap.AddRaw(ScriptComp, &UScriptComponent::HanldeOnCollisionExit);
			Primitive->OnHit.AddRaw(ScriptComp, &UScriptComponent::HanldeOnCollisionHit);
		}
	}	
}

void AActor::EndPlay()
{
	RegisterAllActorTickFunctions(false);
	for (UActorComponent* Component : Components)
	{
		Component->EndPlay();
	}
	bActorHasBegunPlay = false;
}

void AActor::RegisterAllActorTickFunctions(bool bRegister)
{
	if (bRegister && !World)
		return;

	// bCanEverTick이 꺼진 함수는 등록하지 않으므로 정적 메시 액터는 매 프레임 순회 대상에서 빠진다.
	auto Apply = [&](FTickFunction& Function)
	{
		if (bRegister)
			Function.RegisterTickFunction(World->GetTickTaskManager());
		else
			Function.UnRegisterTickFunction();
	};

	Apply(PrimaryActorTick);
	for (UActorComponent* Component : Components)
	{
		if (Component)
			Apply(Component->PrimaryComponentTick);
	}
}

void AActor::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // "Properties"

	FArchive& Ar = Record.GetUnderlyingArchive();

	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_Duplicate))
	{
		// 복제본은 생성자부터 돌아 기본 서브오브젝트(DefaultSceneRoot 등)를 이미 만들어 두었다.
		// 원본에 같은 이름이 있으면 복제가 그 객체를 재활용하지만, 원본에서 지운 경우에는 아래에서
		// Components가 원본 목록으로 덮이며 목록 밖으로 밀려난다. 기억해 두었다가 끝에서 지운다.
		TArray<UActorComponent*> ConstructedComponents;
		if (Ar.IsLoading())
			ConstructedComponents = Components;

		int32 Num = Components.Num();
		FStructuredArchive::FArray ActorArray = Record.EnterArray("Components", Num);
		if (Ar.IsLoading())
			Components.SetNum(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			UObject* Obj = Components[i];
			ActorArray.EnterElement() << Obj;
			Components[i] = static_cast<UActorComponent*>(Obj);
		}

		UObject* WorldObj = World;
		UObject* LevelObj = Level;
		UObject* RootObj = RootComponent;
		Record << SA_VALUE("OwningWorld", WorldObj);
		Record << SA_VALUE("OwningLevel", LevelObj);
		Record << SA_VALUE("RootComponent", RootObj);
		World = static_cast<UWorld*>(WorldObj);
		Level = static_cast<ULevel*>(LevelObj);
		RootComponent = static_cast<USceneComponent*>(RootObj);

		// 목록 밖으로 밀려난 컴포넌트는 액터가 지워질 때 함께 지워지지 않는다.
		// 남겨 두면 PIE 종료 후 해제된 액터를 Owner로 가리키다가, 에디터 종료 때
		// ~UActorComponent의 Owner->RemoveOwnedComponent에서 해제된 메모리를 읽는다.
		for (UActorComponent* Constructed : ConstructedComponents)
		{
			if (Constructed && Components.Find(Constructed) == -1)
				delete Constructed;
		}
		return;
	}

	TArray<UActorComponent*> SavedComponents;
	if (Ar.IsSaving())
	{
		for (UActorComponent* Component : Components)
		{
			if (Component)
			{
				SavedComponents.Add(Component);
			}

		}
	}

	struct FPendingAttachment
	{
		USceneComponent* Child;
		FString ParentName;
	};
	TArray<FPendingAttachment> PendingAttachments;

	// 생성자가 만든 루트. 루트를 바꾸고 이것을 지운 채 저장했다면 로드 후 떠도는 컴포넌트로 남으니 정리한다.
	USceneComponent* ConstructedRoot = RootComponent;
	bool bConstructedRootLoaded = false;

	int32 Num = SavedComponents.Num();
	FStructuredArchive::FArray ComponentArray = Record.EnterArray("Components", Num);
	for (int32 i = 0; i < Num; ++i)
	{
		FStructuredArchive::FRecord ComponentRecord = ComponentArray.EnterElement().EnterRecord();
		FString ParentName;

		if (Ar.IsSaving())
		{
			UActorComponent* Component = SavedComponents[i];
			FString Name = Component->GetName();
			FString ClassName = Component->GetClass()->Name;
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				ParentName = SceneComponent->GetAttachParent() ? SceneComponent->GetAttachParent()->GetName() : "";
			}
			ComponentRecord << SA_VALUE("Name", Name) << SA_VALUE("Class", ClassName) << SA_VALUE("Parent", ParentName);
			Component->Serialize(ComponentRecord);
			continue;
		}

		FString Name, ClassName;
		ComponentRecord << SA_VALUE("Name", Name) << SA_VALUE("Class", ClassName) << SA_VALUE("Parent", ParentName);

		UActorComponent* Component = FindComponentByName(FName(Name));
		if (!Component)
		{
			UClass* ComponentClass = FindClass(ClassName);

			if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				HTR_LOG(Warning, "Load: unknown component class '{}'", ClassName);
				continue;
			}

			Component = AddComponent(ComponentClass, FName(Name));
		}

		if (!Component || Component->GetClass()->Name != ClassName)
		{
			HTR_LOG(Warning, "Load: component {} ({}) failed", Name, ClassName);
			continue;
		}

		if (Component == ConstructedRoot)
			bConstructedRootLoaded = true;

		Component->Serialize(ComponentRecord);
		if(USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (!ParentName.empty())
			{
				PendingAttachments.Add({ SceneComponent, ParentName });
			}
		}
	}

	// 루트를 바꾼 액터도 다시 열었을 때 같은 루트가 되도록 이름을 남긴다.
	// 키가 없는 예전 씬은 생성자가 만든 루트를 그대로 쓴다.
	FString RootName = (Ar.IsSaving() && RootComponent) ? RootComponent->GetName() : FString();
	if (TOptional<FStructuredArchive::FSlot> RootSlot = Record.TryEnterField("RootComponent", true))
		RootSlot.GetValue() << RootName;

	if (Ar.IsLoading() && !RootName.empty())
	{
		// 저장된 루트는 Parent가 비어 있어 Transform이 월드 기준이다.
		// AddComponent가 붙여 둔 부모에서 상대값 그대로 떼어 내야 저장한 위치가 된다.
		// 옛 루트는 아래 PendingAttachments에서 저장된 부모(새 루트) 밑으로 붙는다.
		USceneComponent* SavedRoot = Cast<USceneComponent>(FindComponentByName(FName(RootName)));
		if (SavedRoot && SavedRoot != RootComponent)
		{
			SavedRoot->DetachFromParent(EAttachmentRule::KeepRelative);
			RootComponent = SavedRoot;
		}
	}

	for (const FPendingAttachment& Attachment : PendingAttachments)
	{
		USceneComponent* ParentComponent = Cast<USceneComponent>(FindComponentByName(FName(Attachment.ParentName)));
		if (ParentComponent)
		{
			Attachment.Child->SetupAttachment(ParentComponent);
		}
	}

	if (Ar.IsLoading() && ConstructedRoot && ConstructedRoot != RootComponent && !bConstructedRootLoaded)
	{
		DestroyComponent(ConstructedRoot);
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	for (uint32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == Component)
		{
			Components.RemoveAt(i, 1);
			break;
		}
	}

	if (RootComponent == Component)
	{
		RootComponent = nullptr;
	}
}

bool AActor::ReplaceRootComponent(USceneComponent* NewRoot)
{
	if (!NewRoot || NewRoot == RootComponent || NewRoot->GetOwner() != this)
		return false;

	USceneComponent* OldRoot = RootComponent;

	// 새 루트를 부모에서 떼어 낸다. 화면상 위치는 그대로 둔다.
	NewRoot->DetachFromParent(EAttachmentRule::KeepWorld);

	// 옛 루트는 새 루트 아래로 붙인다. 옛 루트의 자식들도 함께 따라온다.
	if (OldRoot)
		OldRoot->SetupAttachment(NewRoot, EAttachmentRule::KeepWorld);

	RootComponent = NewRoot;
	NewRoot->MarkTransformDirty();

	// 옛 루트를 움직이던 이동 컴포넌트는 새 루트를 움직이게 한다.
	for (UActorComponent* Component : Components)
	{
		UMovementComponent* Movement = Cast<UMovementComponent>(Component);
		if (Movement && Movement->GetUpdatedComponent() == OldRoot)
			Movement->SetUpdatedComponent(NewRoot);
	}

	return true;
}

FVector AActor::GetActorLocation() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldLocation();
	}
	return FVector::ZeroVector;
}


void AActor::SetActorLocation(const FVector& Location)
{
    if (RootComponent)
    {
        RootComponent->SetWorldLocation(Location, false);
    }
}

FRotator AActor::GetActorRotation() const
{
    if (RootComponent)
    {
        // USceneComponent의 GetWorldRotation() 호출
        return RootComponent->GetWorldRotation();
    }
    return FRotator::Identity;
}

void AActor::SetActorRotation(const FRotator& Rotation)
{
	if (RootComponent)
	{
		RootComponent->SetWorldRotation(Rotation, false);
	}
}

FVector AActor::GetActorScale3D() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldScale3D();
	}
	return FVector::OneVector;
}

FQuat AActor::GetActorQuat() const
{
	return GetActorRotation().Quaternion();
}

void AActor::SetActorTransform(const FTransform& Transform)
{
	if (!RootComponent)
	{
		return;
	}

	RootComponent->SetTransform(Transform);
}

//FQuat AActor::GetActorQuat() const
//{
//    if (RootComponent)
//    {
//        return FQuat(RootComponent->GetWorldRotation());
//    }
//    return FQuat::Identity;
//}

FTransform AActor::GetActorTransform() const
{
	if (RootComponent)
	{
		return FTransform(
			RootComponent->GetWorldLocation(),
			RootComponent->GetWorldScale3D(),
			RootComponent->GetWorldRotation()
		);

		// return FTransform(RootComponent->GetWorldMatrix());
	}
	return FTransform::Identity;
}

bool AActor::Destroy()
{
	if (!World)
		return false;

	//지연 파괴
	return World->RequestDestroyActor(this);
	//return World->DestroyActor(this);
}

UActorComponent* AActor::AddComponent(UClass* ComponentClass, FName Name)
{
	if (!ComponentClass 
		|| !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return nullptr;
	}


    if (FName(Name) == FName("None"))
    {
        const FString BaseName = ComponentClass->Name;

        for (uint32 Index = 0; ; ++Index)
        {
            const FName Candidate(BaseName + "_" + std::to_string(Index));

            if (!FindComponentByName(Candidate))
            {
                Name = Candidate;
                break;
            }
        }
    }

	UActorComponent* NewComponent = NewObject<UActorComponent>(this, ComponentClass, Name);
	if (!NewComponent)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(NewComponent))
	{
		World->GetScene().AddPrimitive(Primitive);
		World->GetPhysicsScene().AddPrimitive(Primitive);

		//콜리전 컴포넌트에 ScriptComponent함수를 바인딩
		for (auto* Component : Components)
		{
			UScriptComponent* ScriptComp = Cast<UScriptComponent>(Component);
			if (!ScriptComp)
			{
				continue;
			}

			Primitive->OnBeginOverlap.AddRaw(ScriptComp, &UScriptComponent::HanldeOnCollisionEnter);
			Primitive->OnEndOverlap.AddRaw(ScriptComp, &UScriptComponent::HanldeOnCollisionExit);
			Primitive->OnHit.AddRaw(ScriptComp, &UScriptComponent::HanldeOnCollisionHit);
		}
	}

	//ScriptComponent가 추가될 때
	if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(NewComponent))
	{
		for (auto* Component : Components)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (!Primitive)
			{
				continue;
			}

			Primitive->OnBeginOverlap.AddRaw(ScriptComponent, &UScriptComponent::HanldeOnCollisionEnter);
			Primitive->OnEndOverlap.AddRaw(ScriptComponent, &UScriptComponent::HanldeOnCollisionExit);
			Primitive->OnHit.AddRaw(ScriptComponent, &UScriptComponent::HanldeOnCollisionHit);
		}
	}
	
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(NewComponent))
	{
		SceneComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);        
	}

	NewComponent->SetOwner(this);    
	Components.Add(NewComponent);
	NewComponent->InitializeComponent();

	//if (UParticleSubUVComponent* ParticleSubUV = Cast<UParticleSubUVComponent>(NewComponent))
	//{
	//    ParticleSubUV->BeginPlay();
	//    RegisterAllActorTickFunctions(true);
	//}

	if (bActorHasBegunPlay)
	{
		NewComponent->BeginPlay();
		RegisterAllActorTickFunctions(true);
	}

	return NewComponent;
}

void AActor::DestroyComponent(UActorComponent* Component)
{
	if (!Component)
		return;

	if (Component == RootComponent)
	{
		HTR_LOG(Warning, "Root component cannot be deleted.");
		return;
	}

	if (USceneComponent* Parent = Cast<USceneComponent>(Component))
	{
		TArray<USceneComponent*> Children = Parent->GetAttachChildren();
		for (USceneComponent* Candidate : Children)
		{
			if (!Candidate) { continue; }

			Candidate->DetachFromParent(EAttachmentRule::KeepWorld);
			Candidate->SetupAttachment(Parent->GetAttachParent(), EAttachmentRule::KeepWorld);
		}
	}

	//ScriptComponent가 제거된다면 Pimitive들에 바인딩을 제거한다.
	if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component))
	{
		for (auto* Component : Components)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (!Primitive)
			{
				continue;
			}
			
			Primitive->OnBeginOverlap.RemoveAll(ScriptComponent);
			Primitive->OnEndOverlap.RemoveAll(ScriptComponent);
			Primitive->OnHit.RemoveAll(ScriptComponent);
		}
	}
	else if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
	{
		World->GetScene().RemovePrimitive(Primitive);
        World->GetPhysicsScene().RemovePrimitive(Primitive);
	}
	else if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
	{
		World->GetScene().RemoveFogInfo(Fog->GetUUID());
	}
	else if (ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
	{
		World->GetScene().RemoveLight(Light);
	}
	RemoveOwnedComponent(Component);
	delete Component;
	return;
}

void AActor::OnDefaultSubobjectCreated(UObject* Subobject)
{
	if (UActorComponent* Component = Cast<UActorComponent>(Subobject))
	{
		Component->SetOwner(this);
		Components.Add(Component);
	}
}

UActorComponent* AActor::FindComponentByName(FName Name) const
{
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetFName() == Name)
		{
			return Component;
		}
	}

	// 데칼 액터의 이전 기본 컴포넌트 이름 호환
	if (GetClass()->IsChildOf(ADecalActor::StaticClass()))
	{
		if (Name == FName("UDecalComponent"))
		{
			return FindComponentByName(FName("DecalComponent"));
		}
		if (Name == FName("UBillboardComponent"))
		{
			return FindComponentByName(FName("DecalBillboard"));
		}
	}
	return nullptr;
}

UActorComponent* AActor::FindComponentByClass(const UClass* Clss) const
{
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->IsA(Clss))
		{
			return Component;
		}
	}

	return nullptr;
}

bool AActor::IsOverlappingActor(const AActor* Other) const
{
	if (!Other || Other == this) return false;

	// 내 Primitive 중 하나라도 Other와 겹쳐 있으면 true (UE와 같은 방식)
	for (UActorComponent* OwnedComp : Components)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp);
		if (PrimComp && PrimComp->GetOverlapComps().Num() > 0 && PrimComp->IsOverlappingActor(Other))
		{
			return true;
		}
	}
	return false;
}

bool AActor::TryGetActorBounds(FBox& OutBounds) const
{
	bool bHasBounds = false;

	for (UActorComponent* Component : Components)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive)
			continue;

		FBox PrimitiveBounds = Primitive->CalcBounds();
		if (bHasBounds)
		{
			OutBounds.Expand(PrimitiveBounds);
		}
		else
		{
			OutBounds = PrimitiveBounds;
			bHasBounds = true;
		}
	}

	return bHasBounds;
}

FMinimalViewInfo AActor::CalcCamera(float DeltaTime)
{
	for (UActorComponent* Component : Components)
	{
		UCameraComponent* Camera = Cast<UCameraComponent>(Component);
		if (Camera && Camera->IsActive())
		{
			return Camera->GetCameraView();
		}
	}

	return {
		.Location = GetActorLocation(),
		.Rotation = GetActorRotation(),
	};
}
