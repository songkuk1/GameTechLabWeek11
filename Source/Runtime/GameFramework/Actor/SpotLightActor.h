#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/SpotLightComponent.h"
#include "SpotLightActor.generated.h"

UCLASS()
class ASpotLightActor final : public AActor
{
	GENERATED_BODY()

public:
	ASpotLightActor();
	virtual ~ASpotLightActor() override = default;

	USpotLightComponent* GetLightComponent() const { return LightComponent; }

private:
	USpotLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
