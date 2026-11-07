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
	BlendNode.SetSourceNodes(&SequencePlayerA, &SequencePlayerB);
	RootNode = &BlendNode;
}

void UAnimTwoWayBlendInstance::SetAnimations(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB)
{
	SequencePlayerA.SetSequence(InSequenceA);
	SequencePlayerB.SetSequence(InSequenceB);
}

void UAnimTwoWayBlendInstance::SetAlpha(float InAlpha)
{
	BlendNode.SetAlpha(InAlpha);
}

float UAnimTwoWayBlendInstance::GetAlpha() const
{
	return BlendNode.GetAlpha();
}

void UAnimTwoWayBlendInstance::SetPlaybackPosition(float InTime)
{
	SequencePlayerA.SetPlaybackPosition(InTime);
	SequencePlayerB.SetPlaybackPosition(InTime);
}

float UAnimTwoWayBlendInstance::GetCurrentTime() const
{
	return SequencePlayerA.GetCurrentTime();
}

float UAnimTwoWayBlendInstance::GetPlayLength() const
{
	return SequencePlayerA.GetPlayLength();
}

float UAnimTwoWayBlendInstance::GetFrameRate() const
{
	return SequencePlayerA.GetFrameRate();
}

void UAnimTwoWayBlendInstance::SetPlayRate(float InPlayRate)
{
	SequencePlayerA.SetPlayRate(InPlayRate);
	SequencePlayerB.SetPlayRate(InPlayRate);
}

float UAnimTwoWayBlendInstance::GetPlayRate() const
{
	return SequencePlayerA.GetPlayRate();
}

void UAnimTwoWayBlendInstance::SetLooping(bool bInLoop)
{
	SequencePlayerA.SetLooping(bInLoop);
	SequencePlayerB.SetLooping(bInLoop);
}

bool UAnimTwoWayBlendInstance::IsLooping() const
{
	return SequencePlayerA.IsLooping();
}