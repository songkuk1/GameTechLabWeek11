#include "EnginePCH.h"
#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
}

void UAnimInstance::InitializeAnimation(USkeletalMeshComponent* InOwningComponent)
{
	if (bInitialized)
	{
		return;
	}
	OwningComponent = InOwningComponent;
	NativeInitializeAnimation();
	bInitialized = true;
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bInitialized)
	{
		return;
	}

	NativeUpdateAnimation(DeltaSeconds);
	if (!RootNode)
	{
		return;
	}
	FAnimationUpdateContext Context;
	Context.DeltaTime = DeltaSeconds;
	Context.AnimInstance = this;
	RootNode->Update(Context);
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	if (!bInitialized || !RootNode)
	{
		return;
	}
	RootNode->Evaluate(Output);
}

AActor* UAnimInstance::GetOwningActor() const
{
	return OwningComponent ? OwningComponent->GetOwner() : nullptr;
}

UAnimSingleNodeInstance::UAnimSingleNodeInstance()
{
	RootNode = &SequencePlayer;
}

void UAnimSingleNodeInstance::SetAnimation(UAnimSequence* InSequence)
{
	SequencePlayer.SetSequence(InSequence);
}

void UAnimSingleNodeInstance::SetPlaybackPosition(float InTime)
{
	SequencePlayer.SetPlaybackPosition(InTime);
}

float UAnimSingleNodeInstance::GetCurrentTime() const
{
	return SequencePlayer.GetCurrentTime();
}

float UAnimSingleNodeInstance::GetPlayLength() const
{
	return SequencePlayer.GetPlayLength();
}

float UAnimSingleNodeInstance::GetFrameRate() const
{
	return SequencePlayer.GetFrameRate();
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
	SequencePlayer.SetPlayRate(InPlayRate);
}

float UAnimSingleNodeInstance::GetPlayRate() const
{
	return SequencePlayer.GetPlayRate();
}

void UAnimSingleNodeInstance::SetLooping(bool bInLoop)
{
	SequencePlayer.SetLooping(bInLoop);
}

bool UAnimSingleNodeInstance::IsLooping() const
{
	return SequencePlayer.IsLooping();
}

UAnimTwoWayBlendInstance::UAnimTwoWayBlendInstance()
{
	RootNode = &BlendPlayer;
}

void UAnimTwoWayBlendInstance::SetAnimations(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB)
{
	BlendPlayer.SetAnimations(InSequenceA, InSequenceB);
}

void UAnimTwoWayBlendInstance::SetAlpha(float InAlpha)
{
	BlendPlayer.SetAlpha(InAlpha);
}

float UAnimTwoWayBlendInstance::GetAlpha() const
{
	return BlendPlayer.GetAlpha();
}

void UAnimTwoWayBlendInstance::SetPlaybackPosition(float InTime)
{
	BlendPlayer.SetPlaybackPosition(InTime);
}

float UAnimTwoWayBlendInstance::GetCurrentTime() const
{
	return BlendPlayer.GetCurrentTime();
}

float UAnimTwoWayBlendInstance::GetPlayLength() const
{
	return BlendPlayer.GetPlayLength();
}

float UAnimTwoWayBlendInstance::GetFrameRate() const
{
	return BlendPlayer.GetFrameRate();
}

void UAnimTwoWayBlendInstance::SetPlayRate(float InPlayRate)
{
	BlendPlayer.SetPlayRate(InPlayRate);
}

float UAnimTwoWayBlendInstance::GetPlayRate() const
{
	return BlendPlayer.GetPlayRate();
}

void UAnimTwoWayBlendInstance::SetLooping(bool bInLoop)
{
	BlendPlayer.SetLooping(bInLoop);
}

bool UAnimTwoWayBlendInstance::IsLooping() const
{
	return BlendPlayer.IsLooping();
}

UAnimBlendSpaceInstance::UAnimBlendSpaceInstance()
{
	RootNode = &BlendSpacePlayer;
}

void UAnimBlendSpaceInstance::SetBlendSpace(UBlendSpace1D* InBlendSpace)
{
	BlendSpacePlayer.SetBlendSpace(InBlendSpace);

	BlendSpacePlayer.SetPlayRate(PlayRate);
	BlendSpacePlayer.SetLooping(bLoop);
}

void UAnimBlendSpaceInstance::SetBlendInput(float InInput)
{
	BlendSpacePlayer.SetBlendInput(InInput);
}

float UAnimBlendSpaceInstance::GetBlendInput() const
{
	return BlendSpacePlayer.GetBlendInput();
}

void UAnimBlendSpaceInstance::SetPlaybackPosition(float InTime)
{
	BlendSpacePlayer.SetPlaybackPosition(InTime);
}

float UAnimBlendSpaceInstance::GetCurrentTime() const
{
	return BlendSpacePlayer.GetCurrentTime();
}

float UAnimBlendSpaceInstance::GetPlayLength() const
{
	return BlendSpacePlayer.GetPlayLength();
}

float UAnimBlendSpaceInstance::GetFrameRate() const
{
	return BlendSpacePlayer.GetFrameRate();
}

void UAnimBlendSpaceInstance::SetPlayRate(float InPlayRate)
{
	PlayRate = std::isfinite(InPlayRate) ? InPlayRate : 1.0f;

	BlendSpacePlayer.SetPlayRate(PlayRate);
}

float UAnimBlendSpaceInstance::GetPlayRate() const
{
	return PlayRate;
}

void UAnimBlendSpaceInstance::SetLooping(bool bInLoop)
{
	bLoop = bInLoop;
	BlendSpacePlayer.SetLooping(bLoop);
}

bool UAnimBlendSpaceInstance::IsLooping() const
{
	return bLoop;
}