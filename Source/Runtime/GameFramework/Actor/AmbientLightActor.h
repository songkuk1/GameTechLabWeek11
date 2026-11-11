#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/AmbientLightComponent.h"
#include "AmbientLightActor.generated.h"

UCLASS()
class AAmbientLightActor final : public AActor
{
	GENERATED_BODY()

public:
	AAmbientLightActor();
	virtual ~AAmbientLightActor() override = default;

	UAmbientLightComponent* GetLightComponent() const { return LightComponent; }

private:
	UAmbientLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
