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
	UpdatePose(CurrentPlayTime, LocalPose);
}

void UAnimationInstance::UpdatePose(float deltaTime, TArray<FMatrix>& LocalPose)
{
	Sequence->CalculateCurrentPose(deltaTime, LocalPose);
}
