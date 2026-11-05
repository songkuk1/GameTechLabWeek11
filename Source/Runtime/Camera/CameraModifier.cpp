#include "EnginePCH.h"
#include "CameraModifier.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "PlayerCameraManager.h"

UCameraModifier::UCameraModifier()
{
}

UCameraModifier::~UCameraModifier()
{
	// PCM보다 먼저 지워져도(종료 시 전체 삭제 등) PCM 목록에 dangling이 남지 않게
	if (CameraOwner)
		CameraOwner->RemoveModifierFromList(this);
}

bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	//Alpha 갱신
	UpdateAlpha(DeltaTime);

	//서서히 꺼지는 중이였고 다 줄어들었으면 끈다
	if (bPendingDisable && Alpha <= 0.f)
	{
		DisableModifier(true);
	}

	//기본 Modifier는 POV를 바꾸지 않는다
	return false;
}

void UCameraModifier::EnableModifier()
{
	bDisabled = false;
	bPendingDisable = false;
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
		Alpha = 0.f;   // 다시 켤 때 0에서 서서히 들어오도록
	}
	else if (!bDisabled)
	{
		bPendingDisable = true;
	}
}

void UCameraModifier::AddedToCamera(APlayerCameraManager* PCM)
{
	CameraOwner = PCM;
}

bool UCameraModifier::IsDisabled() const
{
	return bDisabled;
}

uint8 UCameraModifier::GetPriority() const
{
	return Priority;
}

float UCameraModifier::GetTargetAlpha() const
{
	return bPendingDisable ? 0.f : 1.f;
}

float UCameraModifier::GetAlpha() const
{
	return Alpha;
}

APlayerCameraManager* UCameraModifier::GetCameraOwner() const
{
	return CameraOwner;
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
	const float TargetAlpha = GetTargetAlpha();
	const float BlendTime = (TargetAlpha == 0.f) ? AlphaOutTime : AlphaInTime;

	// 블렌드 시간이 없으면 바로 목표값
	if (BlendTime <= 0.f)
	{
		Alpha = TargetAlpha;
		return;
	}

	const float Step = DeltaTime / BlendTime;
	Alpha = (Alpha < TargetAlpha)
		? std::min(Alpha + Step, TargetAlpha)
		: std::max(Alpha - Step, TargetAlpha);
}

