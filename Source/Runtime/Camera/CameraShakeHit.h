#pragma once

#include "CameraShakeBase.h"
#include "UObject/Class.h"

struct FMinimalViewInfo;
class APlayerCameraManager;

class UCameraShakeHit : public UCameraShakeBase
{
	DECLARE_CLASS(UCameraShakeHit, UCameraShakeBase)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraShakeHit();
	virtual ~UCameraShakeHit();
	
};