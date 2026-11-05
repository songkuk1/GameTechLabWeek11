#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

struct FMinimalViewInfo;
class APlayerCameraManager;

class UCameraModifier : public UObject
{
	DECLARE_CLASS(UCameraModifier, UObject)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraModifier();
	virtual ~UCameraModifier();
	
	//true를 반환하면 뒤의 Modifier는 적용하지 않음
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV);

	virtual void AddedToCamera(APlayerCameraManager* PCM);

	//켜기 / 끄기. 끌 때는 즉시 또는 Alpha Out 후
	void EnableModifier();
	void DisableModifier(bool bImmediate = false);

	bool IsDisabled()const;
	uint8 GetPriority()const;

	float GetAlpha() const;
	APlayerCameraManager* GetCameraOwner()const ;

protected:
	float GetTargetAlpha() const;
	virtual void UpdateAlpha(float DeltaTime);

public:
	bool bDebug = false;

	//같은 Priority끼리 하나만 적용
	bool bExclusive = false;

	//적용 순서
	//값이 작을수록 먼저 적용된다. 0이 최우선.
	uint8 Priority = 127;

protected:
	APlayerCameraManager* CameraOwner = nullptr;

	//모디파이어 적용 블렌딩 값
	float Alpha = 0.f;

	float AlphaInTime = 0.f;

	float AlphaOutTime = 0.f;

	bool bDisabled = false;

	//Alpha가 0이 되면 꺼질 예정
	bool bPendingDisable = false;
};