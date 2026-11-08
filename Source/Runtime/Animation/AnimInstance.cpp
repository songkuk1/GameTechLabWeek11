#include "EnginePCH.h"
#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
UAnimInstance::UAnimInstance()
{
	RootNode = &SequencePlayer;
}
UAnimInstance::~UAnimInstance()
{
	ClearAnimationNodes();
}
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

void UAnimInstance::PlayAnimation(UAnimSequence* InSequence)
{
	SequencePlayer.SetSequence(InSequence);
	RootNode = &SequencePlayer;
}

void UAnimInstance::PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha)
{
	TwoWayBlendPlayer.SetAnimations(InSequenceA, InSequenceB);
	TwoWayBlendPlayer.SetAlpha(InAlpha);
	RootNode = &TwoWayBlendPlayer;
}

void UAnimInstance::PlayBlendSpace(UBlendSpace1D* InBlendSpace, float InInput)
{
	BlendSpacePlayer.SetBlendSpace(InBlendSpace);
	BlendSpacePlayer.SetBlendInput(InInput);
	RootNode = &BlendSpacePlayer;
}

void UAnimInstance::SetAnimationBlendAlpha(float InAlpha)
{
	if (RootNode == &TwoWayBlendPlayer)
	{
		TwoWayBlendPlayer.SetAlpha(InAlpha);
	}
}

float UAnimInstance::GetAnimationBlendAlpha() const
{
	return RootNode == &TwoWayBlendPlayer ? TwoWayBlendPlayer.GetAlpha() : 0.0f;
}

void UAnimInstance::SetAnimationBlendInput(float InInput)
{
	if (RootNode == &BlendSpacePlayer)
	{
		BlendSpacePlayer.SetBlendInput(InInput);
	}
}

float UAnimInstance::GetAnimationBlendInput() const
{
	return RootNode == &BlendSpacePlayer ? BlendSpacePlayer.GetBlendInput() : 0.0f;
}

void UAnimInstance::SetPlaybackPosition(float InTime)
{
	if (IAnimPlaybackControl* Control = GetPlaybackControl())
	{
		Control->SetPlaybackPosition(InTime);
	}
}

float UAnimInstance::GetCurrentTime() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetCurrentTime() : 0.0f;
}

float UAnimInstance::GetPlayLength() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetPlayLength() : 0.0f;
}

float UAnimInstance::GetFrameRate() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetFrameRate() : 0.0f;
}

void UAnimInstance::SetPlayRate(float InPlayRate)
{
	if (IAnimPlaybackControl* Control = GetPlaybackControl())
	{
		Control->SetPlayRate(InPlayRate);
	}
}

float UAnimInstance::GetPlayRate() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->GetPlayRate() : 1.0f;
}

void UAnimInstance::SetLooping(bool bInLoop)
{
	if (IAnimPlaybackControl* Control = GetPlaybackControl())
	{
		Control->SetLooping(bInLoop);
	}
}

bool UAnimInstance::IsLooping() const
{
	const IAnimPlaybackControl* Control = GetPlaybackControl();
	return Control ? Control->IsLooping() : true;
}

void UAnimInstance::SetBoolParameter(const FName& Name, bool Value)
{
	if (Name.IsNone())
	{
		return;
	}
	BoolParameters.Add(Name, Value);
}

bool UAnimInstance::GetBoolParameter(const FName& Name) const
{
	const bool* Value = BoolParameters.Find(Name);
	return Value ? *Value : false;
}

bool UAnimInstance::HasBoolParameter(const FName& Name) const
{
	return BoolParameters.Contains(Name);
}

void UAnimInstance::SetFloatParameter(const FName& Name, float Value)
{
	if (Name.IsNone())
	{
		return;
	}

	FloatParameters.Add(Name, std::isfinite(Value) ? Value : 0.0f);
}

float UAnimInstance::GetFloatParameter(const FName& Name) const
{
	const float* Value = FloatParameters.Find(Name);
	return Value ? *Value : 0.0f;
}

void UAnimInstance::ClearAnimationParameters()
{
	BoolParameters.Reset();
	FloatParameters.Reset();
}

bool UAnimInstance::SetRootNode(FAnimNode_Base* InRootNode)
{
	if (!InRootNode)
	{
		RootNode = nullptr;
		return true;
	}

	if (InRootNode == &SequencePlayer || InRootNode == &TwoWayBlendPlayer || InRootNode == &BlendSpacePlayer)
	{
		RootNode = InRootNode;
		return true;
	}

	if (OwnedAnimationNodes.Find(InRootNode) == -1)
	{
		return false;
	}

	RootNode = InRootNode;
	return true;
}

void UAnimInstance::ClearAnimationNodes()
{
	RootNode = &SequencePlayer;
	SequencePlayer.SetSequence(nullptr);
	for (FAnimNode_Base* Node : OwnedAnimationNodes)
	{
		delete Node;
	}
	OwnedAnimationNodes.Reset();
}
