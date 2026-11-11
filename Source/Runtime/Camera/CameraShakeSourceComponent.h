#pragma once

#include "Component/SceneComponent.h"
#include "UObject/Class.h"
#include "CameraShakeSourceComponent.generated.h"


UCLASS()
class UCameraShakeSourceComponent : public USceneComponent
{
	GENERATED_BODY()
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraShakeSourceComponent();
	virtual ~UCameraShakeSourceComponent();	

public:	
};

