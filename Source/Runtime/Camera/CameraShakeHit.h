#pragma once

#include "CameraShakeBase.h"
#include "UObject/Class.h"
#include "CameraShakeHit.generated.h"

struct FMinimalViewInfo;
class APlayerCameraManager;

UCLASS()
class UCameraShakeHit : public UCameraShakeBase
{
	GENERATED_BODY()
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraShakeHit();
	virtual ~UCameraShakeHit();
	
};