#pragma once

#include "AnimNode_Base.h"
#include "Containers/Array.h"
#include "Core/NameTypes.h"

enum class EAnimTransitionRule : uint8
{
	Always,
	BoolTrue,
	BoolFalse,
	StateFinished
};

struct FAnimStateTransition
{
	FName FromState;
	FName ToState;
	FName ParameterName;
	EAnimTransitionRule Rule = EAnimTransitionRule::Always;
	int32 Priority = 0;
	bool bAnyState = false;
};

struct FAnimNode_StateMachine : public FAnimNode_Base
{
public:
	bool AddState(const FName& StateName, FAnimNode_Base* StateNode);
	bool SetState(const FName& StateName, bool bResetPlayback = true);
	FName GetCurrentStateName() const;
	bool IsCurrentStateFinished() const;
	bool AddTransition(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName = NAME_None, int32 Priority = 0);
	bool AddAnyStateTransition(const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName = NAME_None, int32 Priority = 0);
	void ClearTransitions();
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	IAnimPlaybackControl* GetPlaybackControl() override;
	const IAnimPlaybackControl* GetPlaybackControl() const override;
private:
	struct FAnimStateEntry
	{
		FName Name;
		FAnimNode_Base* Node = nullptr;
	};
	int32 FindStateIndex(const FName& StateName) const;
	FAnimNode_Base* GetActiveStateNode();
	const FAnimNode_Base* GetActiveStateNode() const;
	bool AddTransitionInternal(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, bool bAnyState);
	void EvaluateTransitions(const FAnimationUpdateContext& Context);
	bool IsTransitionRuleMet(const FAnimStateTransition& Transition, const FAnimationUpdateContext& Context) const;
	TArray<FAnimStateEntry> States;
	int32 CurrentStateIndex = -1;
	TArray<FAnimStateTransition> Transitions;
};