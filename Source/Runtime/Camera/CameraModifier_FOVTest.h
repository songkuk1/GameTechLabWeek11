#pragma once

#include "CameraModifier.h"
#include "UObject/Class.h"
#include "CameraModifier_FOVTest.generated.h"

class UCameraShakeBase;

UCLASS()
class UCameraModifier_FOVTest : public UCameraModifier
{
	GENERATED_BODY()
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

