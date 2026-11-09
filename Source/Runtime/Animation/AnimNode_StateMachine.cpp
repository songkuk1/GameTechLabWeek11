#include "EnginePCH.h"
#include "AnimNode_StateMachine.h"
#include "Animation/AnimInstance.h"
#include <limits>
#include <algorithm>
#include <cmath>

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
	bIsBlending = false;
	TransitionBlendDuration = 0.0f;
	TransitionBlendElapsed = 0.0f;
	TransitionBlendNode.SetSourceNodes(nullptr, nullptr);
	TransitionBlendNode.SetAlpha(0.0f);
	bHasLastTransition = false;
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

bool FAnimNode_StateMachine::RequestState(const FName& StateName)
{
	if (StateName.IsNone() || FindStateIndex(StateName) == -1)
	{
		return false;
	}

	if (StateName == GetCurrentStateName())
	{
		return true;
	}

	if (bIsBlending)
	{
		return false;
	}

	const FName CurrentState = GetCurrentStateName();
	int32 SelectedTransitionIndex = -1;
	int32 BestPriority = std::numeric_limits<int32>::min();

	for (int32 Index = 0; Index < Transitions.Num(); ++Index)
	{
		const FAnimStateTransition& Transition = Transitions[Index];

		if (Transition.ToState != StateName)
		{
			continue;
		}

		if (!Transition.bAnyState && Transition.FromState != CurrentState)
		{
			continue;
		}

		if (Transition.Priority > BestPriority)
		{
			BestPriority = Transition.Priority;
			SelectedTransitionIndex = Index;
		}
	}

	return SelectedTransitionIndex != -1 && TransitionTo(Transitions[SelectedTransitionIndex]);
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

bool FAnimNode_StateMachine::AddTransition(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold)
{
	return AddTransitionInternal(FromState, ToState, Rule, ParameterName, Priority, BlendDuration, FloatThreshold, false);
}

bool FAnimNode_StateMachine::AddAnyStateTransition(const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold)
{
	return AddTransitionInternal(NAME_None, ToState, Rule, ParameterName, Priority, BlendDuration, FloatThreshold, true);
}

void FAnimNode_StateMachine::ClearTransitions()
{
	Transitions.Reset();
}

bool FAnimNode_StateMachine::HasState(const FName& StateName) const
{
	return FindStateIndex(StateName) != -1;
}

void FAnimNode_StateMachine::GetDebugInfo(FAnimStateMachineDebugInfo& OutInfo) const
{
	OutInfo.CurrentStateName = GetCurrentStateName();
	OutInfo.bCurrentStateFinished = IsCurrentStateFinished();
	OutInfo.bIsBlending = bIsBlending;

	OutInfo.bHasLastTransition = bHasLastTransition;

	if (bHasLastTransition)
	{
		OutInfo.bLastTransitionWasAnyState = LastTransition.bAnyState;

		OutInfo.LastTransitionFromState = LastTransition.FromState;

		OutInfo.LastTransitionToState = LastTransition.ToState;

		OutInfo.LastTransitionRule = LastTransition.Rule;
	}
	else
	{
		OutInfo.bLastTransitionWasAnyState = false;
		OutInfo.LastTransitionFromState = NAME_None;
		OutInfo.LastTransitionToState = NAME_None;
		OutInfo.LastTransitionRule = EAnimTransitionRule::Always;
	}

	OutInfo.TransitionElapsedTime = TransitionBlendElapsed;
	OutInfo.TransitionDuration = TransitionBlendDuration;
	OutInfo.TransitionAlpha = GetTransitionAlpha();
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	if (bIsBlending)
	{
		const float DeltaTime = std::isfinite(Context.DeltaTime) ? std::max(Context.DeltaTime, 0.0f) : 0.0f;
		TransitionBlendElapsed += DeltaTime;

		const float Alpha = std::clamp(TransitionBlendElapsed / TransitionBlendDuration, 0.0f, 1.0f);
		TransitionBlendNode.SetAlpha(Alpha);

		TransitionBlendNode.Update(Context);
		if (Alpha >= 1.0f)
		{
			bIsBlending = false;
			TransitionBlendElapsed = TransitionBlendDuration;
			TransitionBlendNode.SetSourceNodes(nullptr, nullptr);
			TransitionBlendNode.SetAlpha(0.0f);
		}
		return;
	}
	FAnimNode_Base* ActiveNode = GetActiveStateNode();
	if (ActiveNode)
	{
		ActiveNode->Update(Context);
	}

	if (!bExternalStateControl)
	{
		EvaluateTransitions(Context);
	}
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	if (bIsBlending)
	{
		TransitionBlendNode.Evaluate(Output);
		return;
	}

	FAnimNode_Base* ActiveNode = GetActiveStateNode();

	if (ActiveNode)
	{
		ActiveNode->Evaluate(Output);
	}
}

bool FAnimNode_StateMachine::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
	if (bIsBlending)
	{
		return TransitionBlendNode.EvaluateCurve(CurveName, LayerName, OutValue);
	}

	const FAnimNode_Base* ActiveNode = GetActiveStateNode();
	return ActiveNode && ActiveNode->EvaluateCurve(CurveName, LayerName, OutValue);
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

bool FAnimNode_StateMachine::AddTransitionInternal(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold, bool bAnyState)
{
	if (ToState.IsNone() || FindStateIndex(ToState) == -1)
	{
		return false;
	}
	if (!bAnyState && (FromState.IsNone() || FindStateIndex(FromState) == -1))
	{
		return false;
	}

	const bool bUsesBoolParameter = Rule == EAnimTransitionRule::BoolTrue || Rule == EAnimTransitionRule::BoolFalse;

	const bool bUsesFloatParameter = Rule == EAnimTransitionRule::FloatGreaterThan || Rule == EAnimTransitionRule::FloatLessThan || Rule == EAnimTransitionRule::FloatGreaterOrEqual || Rule == EAnimTransitionRule::FloatLessOrEqual;

	if ((bUsesBoolParameter || bUsesFloatParameter) && ParameterName.IsNone())
	{
		return false;
	}

	if (bUsesFloatParameter && !std::isfinite(FloatThreshold))
	{
		return false;
	}

	FAnimStateTransition Transition;
	Transition.FromState = FromState;
	Transition.ToState = ToState;
	Transition.ParameterName = ParameterName;
	Transition.Rule = Rule;
	Transition.Priority = Priority;
	Transition.BlendDuration = std::isfinite(BlendDuration) ? std::max(BlendDuration, 0.0f) : 0.0f;
	Transition.FloatThreshold = FloatThreshold;
	Transition.bAnyState = bAnyState;

	Transitions.Add(Transition);

	return true;
}

bool FAnimNode_StateMachine::TransitionTo(const FAnimStateTransition& Transition)
{
	FAnimNode_Base* PreviousNode = GetActiveStateNode();

	if (!SetState(Transition.ToState, true))
	{
		return false;
	}

	LastTransition = Transition;
	bHasLastTransition = true;

	FAnimNode_Base* CurrentNode = GetActiveStateNode();

	if (PreviousNode && CurrentNode && PreviousNode != CurrentNode && Transition.BlendDuration > 0.0f)
	{
		TransitionBlendNode.SetSourceNodes(PreviousNode, CurrentNode);
		TransitionBlendNode.SetAlpha(0.0f);
		TransitionBlendDuration = Transition.BlendDuration;
		TransitionBlendElapsed = 0.0f;
		bIsBlending = true;
	}

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
		const FAnimStateTransition& Transition = Transitions[SelectedTransitionIndex];
		FAnimNode_Base* PreviousNode = GetActiveStateNode();

		if (SelectedTransitionIndex != -1)
		{
			TransitionTo(Transitions[SelectedTransitionIndex]);
		}
	}
}

bool FAnimNode_StateMachine::IsTransitionRuleMet(const FAnimStateTransition& Transition, const FAnimationUpdateContext& Context) const
{
	switch (Transition.Rule)
	{
	case EAnimTransitionRule::Always:
	{
		return true;
	}
	case EAnimTransitionRule::BoolTrue:
	{
		return Context.AnimInstance && Context.AnimInstance->HasBoolParameter(Transition.ParameterName) && Context.AnimInstance->GetBoolParameter(Transition.ParameterName);
	}
	case EAnimTransitionRule::BoolFalse:
	{
		return Context.AnimInstance && Context.AnimInstance->HasBoolParameter(Transition.ParameterName) && !Context.AnimInstance->GetBoolParameter(Transition.ParameterName);
	}
	case EAnimTransitionRule::StateFinished:
	{
		return IsCurrentStateFinished();
	}
	case EAnimTransitionRule::FloatGreaterThan:
	{
		if (!Context.AnimInstance || !Context.AnimInstance->HasFloatParameter(Transition.ParameterName))
		{
			return false;
		}
		return Context.AnimInstance->GetFloatParameter(Transition.ParameterName) > Transition.FloatThreshold;
	}

	case EAnimTransitionRule::FloatLessThan:
	{
		if (!Context.AnimInstance || !Context.AnimInstance->HasFloatParameter(Transition.ParameterName))
		{
			return false;
		}
		return Context.AnimInstance->GetFloatParameter(Transition.ParameterName) < Transition.FloatThreshold;
	}

	case EAnimTransitionRule::FloatGreaterOrEqual:
	{
		if (!Context.AnimInstance || !Context.AnimInstance->HasFloatParameter(Transition.ParameterName))
		{
			return false;
		}
		return Context.AnimInstance->GetFloatParameter(Transition.ParameterName) >= Transition.FloatThreshold;
	}

	case EAnimTransitionRule::FloatLessOrEqual:
	{
		if (!Context.AnimInstance || !Context.AnimInstance->HasFloatParameter(Transition.ParameterName))
		{
			return false;
		}
		return Context.AnimInstance->GetFloatParameter(Transition.ParameterName) <= Transition.FloatThreshold;
	}
		
	}
	return false;
}
