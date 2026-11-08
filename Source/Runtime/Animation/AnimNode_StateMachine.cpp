#include "EnginePCH.h"
#include "AnimNode_StateMachine.h"
#include "Animation/AnimInstance.h"
#include <limits>

bool FAnimNode_StateMachine::AddState(const FName& StateName, FAnimNode_Base* StateNode)
{
	if (StateName.IsNone() || !StateNode)
	{
		return false;
	}
	if (FindStateIndex(StateName) != -1)
	{
		return false;
	}
	FAnimStateEntry Entry;
	Entry.Name = StateName;
	Entry.Node = StateNode;
	States.Add(Entry);
	return true;
}

bool FAnimNode_StateMachine::SetState(const FName& StateName, bool bResetPlayback)
{
	const int32 NewIndex = FindStateIndex(StateName);
	if (NewIndex == -1)
	{
		return false;
	}
	CurrentStateIndex = NewIndex;
	if (bResetPlayback)
	{
		if (IAnimPlaybackControl* Playback = GetPlaybackControl())
		{
			const float StartTime = Playback->GetPlayRate() < 0.0f ? Playback->GetPlayLength() : 0.0f;
			Playback->SetPlaybackPosition(StartTime);
		}
	}
	return true;
}

FName FAnimNode_StateMachine::GetCurrentStateName() const
{
	if (!States.IsValidIndex(CurrentStateIndex))
	{
		return NAME_None;
	}
	return States[CurrentStateIndex].Name;
}

bool FAnimNode_StateMachine::IsCurrentStateFinished() const
{
	const IAnimPlaybackControl* Playback = GetPlaybackControl();
	if (!Playback || Playback->IsLooping())
	{
		return false;
	}

	const float PlayLength = Playback->GetPlayLength();
	if (PlayLength <= 0.0f)
	{
		return false;
	}
	const float CurrentTime = Playback->GetCurrentTime();
	const float PlayRate = Playback->GetPlayRate();

	if (PlayRate > 0.0f)
	{
		return CurrentTime >= PlayLength;
	}

	if (PlayRate < 0.0f)
	{
		return CurrentTime <= 0.0f;
	}

	return false;
}

bool FAnimNode_StateMachine::AddTransition(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority)
{
	return AddTransitionInternal(FromState, ToState, Rule, ParameterName, Priority, false);
}

bool FAnimNode_StateMachine::AddAnyStateTransition(const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority)
{
	return AddTransitionInternal(NAME_None, ToState, Rule, ParameterName, Priority, true);
}

void FAnimNode_StateMachine::ClearTransitions()
{
	Transitions.Reset();
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	EvaluateTransitions(Context);
	FAnimNode_Base* ActiveNode = GetActiveStateNode();

	if (ActiveNode)
	{
		ActiveNode->Update(Context);
	}
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	FAnimNode_Base* ActiveNode = GetActiveStateNode();

	if (ActiveNode)
	{
		ActiveNode->Evaluate(Output);
	}
}

IAnimPlaybackControl* FAnimNode_StateMachine::GetPlaybackControl()
{
	FAnimNode_Base* ActiveNode = GetActiveStateNode();
	return ActiveNode ? ActiveNode->GetPlaybackControl() : nullptr;
}

const IAnimPlaybackControl* FAnimNode_StateMachine::GetPlaybackControl() const
{
	const FAnimNode_Base* ActiveNode = GetActiveStateNode();
	return ActiveNode ? ActiveNode->GetPlaybackControl() : nullptr;
}

int32 FAnimNode_StateMachine::FindStateIndex(const FName& StateName) const
{
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		if (States[Index].Name == StateName)
		{
			return Index;
		}
	}
	return -1;
}

FAnimNode_Base* FAnimNode_StateMachine::GetActiveStateNode()
{
	if (!States.IsValidIndex(CurrentStateIndex))
	{
		return nullptr;
	}
	return States[CurrentStateIndex].Node;
}

const FAnimNode_Base* FAnimNode_StateMachine::GetActiveStateNode() const
{
	if (!States.IsValidIndex(CurrentStateIndex))
	{
		return nullptr;
	}
	return States[CurrentStateIndex].Node;
}

bool FAnimNode_StateMachine::AddTransitionInternal(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, bool bAnyState)
{
	if (ToState.IsNone() || FindStateIndex(ToState) == -1)
	{
		return false;
	}
	if (!bAnyState && (FromState.IsNone() || FindStateIndex(FromState) == -1))
	{
		return false;
	}

	if ((Rule == EAnimTransitionRule::BoolTrue || Rule == EAnimTransitionRule::BoolFalse) && ParameterName.IsNone())
	{
		return false;
	}

	FAnimStateTransition Transition;
	Transition.FromState = FromState;
	Transition.ToState = ToState;
	Transition.ParameterName = ParameterName;
	Transition.Rule = Rule;
	Transition.Priority = Priority;
	Transition.bAnyState = bAnyState;

	Transitions.Add(Transition);

	return true;
}

void FAnimNode_StateMachine::EvaluateTransitions(const FAnimationUpdateContext& Context)
{
	const FName CurrentState = GetCurrentStateName();
	if (CurrentState.IsNone())
	{
		return;
	}
	int32 SelectedTransitionIndex = -1;
	int32 BestPriority = std::numeric_limits<int32>::min();
	for (int32 Index = 0; Index < Transitions.Num(); ++Index)
	{
		const FAnimStateTransition& Transition = Transitions[Index];
		if (!Transition.bAnyState && Transition.FromState != CurrentState)
		{
			continue;
		}
		if (Transition.ToState == CurrentState)
		{
			continue;
		}
		if (!IsTransitionRuleMet(Transition, Context))
		{
			continue;
		}
		if (Transition.Priority > BestPriority)
		{
			BestPriority = Transition.Priority;
			SelectedTransitionIndex = Index;
		}
	}
	if (SelectedTransitionIndex != -1)
	{
		SetState(Transitions[SelectedTransitionIndex].ToState, true);
	}
}

bool FAnimNode_StateMachine::IsTransitionRuleMet(const FAnimStateTransition& Transition, const FAnimationUpdateContext& Context) const
{
	switch (Transition.Rule)
	{
	case EAnimTransitionRule::Always:
		return true;

	case EAnimTransitionRule::BoolTrue:
		return Context.AnimInstance && Context.AnimInstance->HasBoolParameter(Transition.ParameterName) && Context.AnimInstance->GetBoolParameter(Transition.ParameterName);
	case EAnimTransitionRule::BoolFalse:
		return Context.AnimInstance && Context.AnimInstance->HasBoolParameter(Transition.ParameterName) && !Context.AnimInstance->GetBoolParameter(Transition.ParameterName);
	case EAnimTransitionRule::StateFinished:
		return IsCurrentStateFinished();
	}
	return false;
}
