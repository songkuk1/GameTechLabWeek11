#include "EnginePCH.h"
#include "CameraModifier_CameraShake.h"

#include "Camera/CameraTypes.h"
#include "CameraShakeBase.h"

UCameraModifier_CameraShake::UCameraModifier_CameraShake()
{
	//다른 효과가 끝난 뒤 흔드는 게 좋다
	Priority = 100;
}

UCameraModifier_CameraShake::~UCameraModifier_CameraShake()
{
	RemoveAllCameraShakes(true);
}

UCameraShakeBase* UCameraModifier_CameraShake::AddCameraShake(UClass* ShakeClass, float Scale)
{
	if (!ShakeClass || !ShakeClass->IsChildOf(UCameraShakeBase::StaticClass())) return nullptr;

	auto* Shake = NewObject<UCameraShakeBase>(this, ShakeClass);
	if (Shake)
	{
		Shake->StartShake(Scale);
		ActiveShakes.Add(Shake);
		return Shake;
	}
	return nullptr;
}

void UCameraModifier_CameraShake::RemoveAllCameraShakes(bool bImmediate)
{
	if (bImmediate)
	{
		// 복사 후 비운다. 이미 비어 있으므로 ShakeBase 소멸자의 RemoveShakeFromList는 아무것도 안 한다
		TArray<UCameraShakeBase*> ToDelete = ActiveShakes;
		ActiveShakes.Reset();

		for (UCameraShakeBase* Shake : ToDelete)
		{
			delete Shake;
		}
		return;
	}

	// 블렌드 아웃 후 ModifyCamera의 정리 루프가 지운다
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		Shake->StopShake(false);
	}
}

bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		Shake->UpdateAndApply(DeltaTime, Alpha, InOutPOV);
	}

	// 끝난 Shake 정리. 소멸자가 RemoveShakeFromList로 자기를 빼므로 delete만 한다.
	// 뒤에서부터 지워야 아직 보지 않은 앞쪽 인덱스가 밀리지 않는다
	for (int32 i = ActiveShakes.Num() - 1; i >= 0; --i)
	{
		if (ActiveShakes[i]->IsFinished())
		{
			delete ActiveShakes[i];
		}
	}

	return false;
}

void UCameraModifier_CameraShake::RemoveShakeFromList(UCameraShakeBase* Shake)
{
	const int32 Index = ActiveShakes.Find(Shake);
	if (Index != -1)
		ActiveShakes.RemoveAt(Index);
}

