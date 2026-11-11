#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/PointLightComponent.h"
#include "PointLightActor.generated.h"

UCLASS()
class APointLightActor final : public AActor
{
	GENERATED_BODY()

public:
	APointLightActor();
	virtual ~APointLightActor() override = default;

	UPointLightComponent* GetLightComponent() const { return LightComponent; }

private:
	UPointLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
