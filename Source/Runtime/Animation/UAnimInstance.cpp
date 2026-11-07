#include "EnginePCH.h"
#include "UAnimInstance.h"

UAnimationInstance::UAnimationInstance()
{
	CurrentPlayTime = 0;
	Sequence = nullptr;
	bLooping = false;;
}

UAnimationInstance::~UAnimationInstance()
{
	Sequence = nullptr;
}

void UAnimationInstance::Tick(float deltaTime, TArray<FMatrix>& LocalPose)
{
	CurrentPlayTime += deltaTime;
	UpdatePose(CurrentPlayTime, LocalPose, bLooping);

}

void UAnimationInstance::UpdatePose(float CurrentPlayTime, TArray<FMatrix>& LocalPose, bool bLoop)
{
	if (Sequence) Sequence->CalculateCurrentPose(CurrentPlayTime, LocalPose, bLoop);
}
