#pragma once

#include "CameraModifier.h"
#include "UObject/Class.h"

class UCameraShakeBase;

class UCameraModifier_FOVTest : public UCameraModifier
{
	DECLARE_CLASS(UCameraModifier_FOVTest, UCameraModifier)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraModifier_FOVTest();
	virtual ~UCameraModifier_FOVTest();
	
	//true를 반환하면 뒤의 Modifier는 적용하지 않음
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	UCameraShakeBase* ActiveShake = nullptr;
	float ElapsedTime = 0.f;
};

