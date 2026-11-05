#pragma once

#include "CameraShakeBase.h"
#include "UObject/Class.h"

// 적이 공중에서 내려찍을 때(충격파 착지)의 흔들림. 피격보다 무겁고 위아래로 크게 흔든다.
class UCameraShakeSlam : public UCameraShakeBase
{
	DECLARE_CLASS(UCameraShakeSlam, UCameraShakeBase)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraShakeSlam();
	virtual ~UCameraShakeSlam();
};
