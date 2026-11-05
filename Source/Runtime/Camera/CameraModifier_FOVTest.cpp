#include "EnginePCH.h"
#include "CameraModifier_FOVTest.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"

UCameraModifier_FOVTest::UCameraModifier_FOVTest()
{
	Priority = 50;
	AlphaInTime = 1.0f;
	AlphaOutTime = 1.0f;
}

UCameraModifier_FOVTest::~UCameraModifier_FOVTest()
{
}

bool UCameraModifier_FOVTest::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	// 3초 뒤 서서히 끄기
	ElapsedTime += DeltaTime;
	if (ElapsedTime > 3.f)
	{
		DisableModifier(false);
	}

	InOutPOV.FOV += 30.f * Alpha;
	return false;
}

