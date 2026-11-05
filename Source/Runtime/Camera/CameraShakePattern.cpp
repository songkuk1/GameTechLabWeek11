#include "EnginePCH.h"
#include "CameraShakePattern.h"
#include "CameraShakeBase.h"


UCameraShakePattern::UCameraShakePattern()
{
}

UCameraShakePattern::~UCameraShakePattern()
{
	// ShakeBase보다 먼저 지워져도 ShakeBase가 해제된 Pattern을 가리키지 않게
	if (UCameraShakeBase* OwnerShake = Cast<UCameraShakeBase>(GetOuter()))
		OwnerShake->ClearRootPattern(this);
}

void UCameraShakePattern::StartShake()
{
	ElapsedTime = 0.f;
	StopElapsed = 0.f;
	bStopping = false;
	bFinished = false;
	OnStart();
}

void UCameraShakePattern::UpdateShake(float DeltaTime, FCameraShakeUpdateResult& Out)
{
	if (bFinished) return;
	ElapsedTime += DeltaTime;

	float Weight = 1.f;
	if (BlendInTime > 0.f)
	{
		Weight = std::min(Weight, ElapsedTime / BlendInTime);
	}

	// 끝나기 전 BlendOutTime 동안 줄이기 (Duration이 있을 때)
	if (Duration > 0.f)
	{
		const float Remain = Duration - ElapsedTime;

		if (Remain <= 0.f)
		{
			bFinished = true; 
			return;
		}

		if (BlendOutTime > 0.f)
		{
			Weight = std::min(Weight, Remain / BlendOutTime);
		}
	}

	// 수동 정지 블렌드 아웃
	if (bStopping)
	{
		StopElapsed += DeltaTime;
		if (StopElapsed >= BlendOutTime)
		{
			bFinished = true;
			return;
		}
		Weight = std::min(Weight, 1.f - StopElapsed / BlendOutTime);
	}

	OnUpdate(DeltaTime, Out);

	Out.Location = Out.Location * Weight;
	Out.Rotation.Pitch *= Weight; 
	Out.Rotation.Yaw *= Weight; 
	Out.Rotation.Roll *= Weight;
	Out.FOV *= Weight;
}

void UCameraShakePattern::StopShake(bool bImmediate)
{
	if (bImmediate)
	{
		bFinished = true;
	}
	else
	{
		if (BlendOutTime == 0.f)
		{
			bFinished = true;
		}
		else
		{
			bStopping = true;
		}
	}
}

bool UCameraShakePattern::IsFinished() const
{
	return bFinished;
}
