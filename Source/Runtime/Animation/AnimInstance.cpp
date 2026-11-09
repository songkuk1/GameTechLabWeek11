#include "EnginePCH.h"
#include "AnimInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationGraph.h"
#include "Animation/AnimNode_StateMachine.h"
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

bool UAnimInstance::InitializeAnimationGraph(UAnimationGraph* InGraph)
{
    if (!InGraph || !InGraph->IsAnimationGraphDataValid())
    {
        return false;
    }

    ClearAnimationNodes();
    ClearAnimationParameters();

    for (const FAnimGraphParameterDefinition& Parameter : InGraph->GetParameters())
    {
        if (Parameter.Type == EAnimGraphParameterType::Bool)
        {
            SetBoolParameter(Parameter.Name, Parameter.BoolDefaultValue);
        }
        else if (Parameter.Type == EAnimGraphParameterType::Float)
        {
            SetFloatParameter(Parameter.Name, Parameter.FloatDefaultValue);
        }
    }

    FAnimNode_StateMachine* StateMachine = CreateAnimationNode<FAnimNode_StateMachine>();
	StateMachineNode = StateMachine;

    for (const FAnimGraphStateDefinition& State : InGraph->GetStates())
    {
        FAnimNode_Base* StateNode = nullptr;

        if (State.NodeType == EAnimGraphStateNodeType::SequencePlayer)
        {
            FAnimNode_SequencePlayer* Player = CreateAnimationNode<FAnimNode_SequencePlayer>();

            Player->SetSequence(State.Sequence);
            Player->SetPlayRate(State.PlayRate);
            Player->SetLooping(State.bLooping);

            StateNode = Player;
        }
        else if (State.NodeType == EAnimGraphStateNodeType::BlendSpacePlayer)
        {
            FAnimNode_BlendSpacePlayer* Player = CreateAnimationNode<FAnimNode_BlendSpacePlayer>();

            Player->SetBlendSpace(State.BlendSpace);
            Player->SetBlendParameter(State.BlendParameterName);
            Player->SetPlayRate(State.PlayRate);
            Player->SetLooping(State.bLooping);

            StateNode = Player;
        }

        if (!StateNode || !StateMachine->AddState(State.Name, StateNode))
        {
            ClearAnimationNodes();
            ClearAnimationParameters();
            return false;
        }
    }

    for (const FAnimGraphTransitionDefinition& Transition : InGraph->GetTransitions())
    {
        bool bAdded = false;

        if (Transition.bAnyState)
        {
            bAdded = StateMachine->AddAnyStateTransition(Transition.ToState, Transition.Rule, Transition.ParameterName, Transition.Priority, Transition.BlendDuration, Transition.FloatThreshold);
        }
        else
        {
            bAdded = StateMachine->AddTransition(Transition.FromState, Transition.ToState, Transition.Rule, Transition.ParameterName, Transition.Priority, Transition.BlendDuration, Transition.FloatThreshold);
        }

        if (!bAdded)
        {
            ClearAnimationNodes();
            ClearAnimationParameters();
            return false;
        }
    }

    if (!StateMachine->SetState(InGraph->GetEntryState()))
    {
        ClearAnimationNodes();
        ClearAnimationParameters();
        return false;
    }

    if (!SetRootNode(StateMachine))
    {
        ClearAnimationNodes();
        ClearAnimationParameters();
        return false;
    }

    return true;
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bInitialized)
	{
		return;
	}

	PendingAnimNotifyUpdates.Reset();

	NativeUpdateAnimation(DeltaSeconds);

	FAnimationUpdateContext Context;
	Context.DeltaTime = DeltaSeconds;
	Context.AnimInstance = this;

	if (RootNode)
	{
		RootNode->Update(Context);
	}

	TriggerAnimNotifies(DeltaSeconds);
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

bool UAnimInstance::HasFloatParameter(const FName& Name) const
{
	return FloatParameters.Contains(Name);
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
	EndActiveAnimNotifyStates();
	PendingAnimNotifyUpdates.Reset();

	RootNode = &SequencePlayer;
	StateMachineNode = nullptr;

	SequencePlayer.SetSequence(nullptr);

	for (FAnimNode_Base* Node : OwnedAnimationNodes)
	{
		delete Node;
	}

	OwnedAnimationNodes.Reset();
}

bool UAnimInstance::GetStateMachineDebugInfo(FAnimStateMachineDebugInfo& OutInfo) const
{
	if (!StateMachineNode || RootNode != StateMachineNode)
	{
		return false;
	}
	StateMachineNode->GetDebugInfo(OutInfo);
	return true;
}

void UAnimInstance::QueueAnimNotifyUpdate(const FAnimNode_SequencePlayer* Player, UAnimSequence* Sequence, float PreviousTime, float Advance, float CurrentTime, float PlayLength, bool bLooping)
{
	if (!Player || !Sequence || !Sequence->GetDataModel())
	{
		return;
	}

	if (!std::isfinite(PreviousTime) || !std::isfinite(Advance) || !std::isfinite(CurrentTime) || !std::isfinite(PlayLength) || PlayLength <= 0.0f)
	{
		return;
	}

	FQueuedAnimNotifyUpdate Update;
	Update.Player = Player;
	Update.Sequence = Sequence;
	Update.PreviousTime = PreviousTime;
	Update.Advance = Advance;
	Update.CurrentTime = CurrentTime;
	Update.PlayLength = PlayLength;
	Update.bLooping = bLooping;

	PendingAnimNotifyUpdates.Add(std::move(Update));
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
}

void UAnimInstance::EndActiveAnimNotifyStates()
{
	if (OwningComponent)
	{
		for (const FActiveAnimNotifyState& ActiveState : ActiveAnimNotifyStates)
		{
			OwningComponent->HandleAnimNotify(ActiveState.NotifyName, EAnimNotifyEventType::End, 0.0f);
		}
	}
	ActiveAnimNotifyStates.Reset();
}
