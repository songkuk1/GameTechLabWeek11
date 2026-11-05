#pragma once

#include "CameraModifier.h"
#include "UObject/Class.h"

class UCameraShakeBase;

class UCameraModifier_CameraShake : public UCameraModifier
{
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraModifier_CameraShake();
	virtual ~UCameraModifier_CameraShake();
	
	UCameraShakeBase* AddCameraShake(UClass* ShakeClass, float Scale = 1.f);
	void RemoveAllCameraShakes(bool bImmediate);

	// ShakeBase 소멸자가 부른다. delete하지 않고 목록에서만 뺀다
	void RemoveShakeFromList(UCameraShakeBase* Shake);

	//true를 반환하면 뒤의 Modifier는 적용하지 않음
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;


private:
	TArray<UCameraShakeBase*> ActiveShakes;
};

