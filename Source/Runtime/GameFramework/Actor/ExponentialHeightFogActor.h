#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "ExponentialHeightFogActor.generated.h"


UCLASS()
class AExponentialHeightFogActor final : public AActor
{
	GENERATED_BODY()




public:
	AExponentialHeightFogActor();
	virtual ~AExponentialHeightFogActor() override = default;

	AExponentialHeightFogActor(const AExponentialHeightFogActor&) = delete;
	AExponentialHeightFogActor operator=(const AExponentialHeightFogActor&) = delete;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UExponentialHeightFogComponent* GetExponentialHeightFogComponent() const { return ExponentialHeightFogComponent; }

private:
	UExponentialHeightFogComponent* ExponentialHeightFogComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};