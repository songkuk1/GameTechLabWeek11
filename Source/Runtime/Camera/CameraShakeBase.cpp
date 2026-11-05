#include "EnginePCH.h"
#include "CameraShakeBase.h"

#include "CameraShakePattern.h"
#include "CameraModifier_CameraShake.h"
#include "Camera/CameraTypes.h"

UCameraShakeBase::UCameraShakeBase()
{
}

UCameraShakeBase::~UCameraShakeBase()
{
	// 소유한 Modifier 목록에서 자기를 뺀다 (Modifier보다 먼저 지워지는 경우 대비)
	if (UCameraModifier_CameraShake* OwnerModifier = Cast<UCameraModifier_CameraShake>(GetOuter()))
		OwnerModifier->RemoveShakeFromList(this);

	// 먼저 끊고 지운다. Pattern 소멸자의 ClearRootPattern은 아무것도 안 한다
	UCameraShakePattern* Pattern = RootPattern;
	RootPattern = nullptr;
	delete Pattern;
}

void UCameraShakeBase::ClearRootPattern(UCameraShakePattern* Pattern)
{
	if (RootPattern == Pattern)
		RootPattern = nullptr;
}

void UCameraShakeBase::StartShake(float InScale)
{
	ShakeScale = InScale;

	if (RootPattern)
	{
		RootPattern->StartShake();
	}
}

void UCameraShakeBase::StopShake(bool bImmediate)
{
	if (RootPattern)
	{
		RootPattern->StopShake(bImmediate);
	}
}

bool UCameraShakeBase::IsFinished() const
{
	return !RootPattern || RootPattern->IsFinished();
}

void UCameraShakeBase::UpdateAndApply(float DeltaTime, float InAlpha, FMinimalViewInfo& InOutPOV)
{
	if (!RootPattern) return;
	FCameraShakeUpdateResult R;

	RootPattern->UpdateShake(DeltaTime, R);
	const float S = ShakeScale * InAlpha;

	InOutPOV.Location += InOutPOV.Rotation.Quaternion().RotateVector(R.Location * S);
	InOutPOV.Rotation.Pitch += R.Rotation.Pitch * S;
	InOutPOV.Rotation.Yaw += R.Rotation.Yaw * S;
	InOutPOV.Rotation.Roll += R.Rotation.Roll * S;
	InOutPOV.FOV += R.FOV * S;
}
