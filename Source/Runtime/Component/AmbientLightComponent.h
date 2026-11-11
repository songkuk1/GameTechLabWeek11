#pragma once

#include "LightComponentBase.h"
#include "AmbientLightComponent.generated.h"

UCLASS()
class UAmbientLightComponent : public ULightComponentBase
{
	GENERATED_BODY()

public:
	UAmbientLightComponent() = default;
	virtual ~UAmbientLightComponent() override = default;

	FAmbientLightInfo GetAmbientLightInfo() const;
	virtual FLightSceneProxy* CreateSceneProxy() override;
private:
};
