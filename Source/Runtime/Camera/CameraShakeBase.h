#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class UCameraShakePattern;
struct FMinimalViewInfo;

class UCameraShakeBase : public UObject
{
	DECLARE_CLASS(UCameraShakeBase, UObject)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraShakeBase();
	virtual ~UCameraShakeBase();

	void StartShake(float InScale);
	void StopShake(bool bImmediate);
	bool IsFinished() const;

	// Pattern 소멸자가 부른다. delete하지 않고 연결만 끊는다
	void ClearRootPattern(UCameraShakePattern* Pattern);

	// 패턴 결과 × Scale × InAlpha 를 POV에 적용
	void UpdateAndApply(float DeltaTime, float InAlpha, FMinimalViewInfo& InOutPOV);

protected:
	UCameraShakePattern* RootPattern = nullptr;
	float ShakeScale = 1.f;
};