#pragma once

#include "GameFramework/Actor.h"
#include "Component/SkeletalMeshComponent.h"
#include "SkeletalMeshActor.generated.h"

UCLASS()
class ASkeletalMeshActor :public AActor
{
	GENERATED_BODY()
public:
	ASkeletalMeshActor();
	virtual ~ASkeletalMeshActor() = default;

	virtual void BeginPlay() override;

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
	void SetPrimitiveType(EPrimitiveType Type);

private:
	USkeletalMeshComponent* SkeletalMeshComponent;
};

