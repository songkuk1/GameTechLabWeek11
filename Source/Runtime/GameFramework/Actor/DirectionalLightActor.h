#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "DirectionalLightActor.generated.h"

UCLASS()
class ADirectionalLightActor final : public AActor
{
	GENERATED_BODY()

public:
	ADirectionalLightActor();
	virtual ~ADirectionalLightActor() override = default;

	UDirectionalLightComponent* GetLightComponent() const { return LightComponent; }

private:
	UDirectionalLightComponent* LightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
