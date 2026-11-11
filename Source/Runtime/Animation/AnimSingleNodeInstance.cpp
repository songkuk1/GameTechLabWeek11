#include "EnginePCH.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Component/SkeletalMeshComponent.h"

void UAnimSingleNodeInstance::SetAnimation(UAnimationAsset* InAsset)
{
	CurrentAsset = InAsset;
	if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(InAsset))
	{
		PlayAnimation(AnimSeq);
	}
	else
	{
		PlayAnimation(nullptr);
	}
	SetPlaybackPosition(0.0f);
	bPlaying = true;
}

void UAnimSingleNodeInstance::Play()
{
	if (GetPlayRate() < 0.0f)
	{
		SetPlayRate(-GetPlayRate());
	}
	const float Duration = GetPlayLength();
	if (Duration > 0.0f && GetCurrentTime() >= Duration)
	{
		SetPlaybackPosition(0.0f);
	}
	bPlaying = true;
}

void UAnimSingleNodeInstance::PlayReverse()
{
	if (GetPlayRate() > 0.0f)
	{
		SetPlayRate(-GetPlayRate());
	}
	if (GetCurrentTime() <= 0.0f)
	{
		SetPlaybackPosition(GetPlayLength());
	}
	bPlaying = true;
}

void UAnimSingleNodeInstance::StepForward(float StepTime)
{
	Pause();
	const float Length = GetPlayLength();
	SetPosition(std::min(Length, GetCurrentTime() + StepTime));
}

void UAnimSingleNodeInstance::StepBackward(float StepTime)
{
	Pause();
	SetPosition(std::max(0.0f, GetCurrentTime() - StepTime));
}

void UAnimSingleNodeInstance::Stop()
{
	Pause();
	SetPosition(0.0f);
}

void UAnimSingleNodeInstance::SetPosition(float InTime)
{
	SetPlaybackPosition(InTime);
	if (USkeletalMeshComponent* Comp = GetOwningComponent())
	{
		Comp->TickAnimation(0.0f);
	}
}

void UAnimSingleNodeInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bPlaying)
	{
		Super::UpdateAnimation(0.0f);
		return;
	}

	Super::UpdateAnimation(DeltaSeconds);

	if (!IsLooping())
	{
		const float Current = GetCurrentTime();
		const float Duration = GetPlayLength();
		const float Rate = GetPlayRate();
		if (Rate > 0.0f && Current >= Duration)
		{
			bPlaying = false;
		}
		else if (Rate < 0.0f && Current <= 0.0f)
		{
			bPlaying = false;
		}
	}
}
