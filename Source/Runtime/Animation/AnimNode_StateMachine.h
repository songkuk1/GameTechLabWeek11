#pragma once

#include "AnimNode_Base.h"
#include "Animation/AnimNode_TwoWayBlend.h"
#include "Containers/Array.h"
#include "Core/NameTypes.h"

enum class EAnimTransitionRule : uint8
{
	Always,
	BoolTrue,
	BoolFalse,
	StateFinished,
	FloatGreaterThan,
	FloatLessThan,
	FloatGreaterOrEqual,
	FloatLessOrEqual
};

struct FAnimStateTransition
{
	FName FromState;
	FName ToState;
	FName ParameterName;
	EAnimTransitionRule Rule = EAnimTransitionRule::Always;
	int32 Priority = 0;
	float BlendDuration = 0.0f;
	float FloatThreshold = 0.0f;
	bool bAnyState = false;
};

struct FAnimStateMachineDebugInfo
{
	FName CurrentStateName = NAME_None;

	bool bCurrentStateFinished = false;
	bool bIsBlending = false;

	bool bHasLastTransition = false;
	bool bLastTransitionWasAnyState = false;

	FName LastTransitionFromState = NAME_None;
	FName LastTransitionToState = NAME_None;

	EAnimTransitionRule LastTransitionRule = EAnimTransitionRule::Always;

	float TransitionElapsedTime = 0.0f;
	float TransitionDuration = 0.0f;
	float TransitionAlpha = 1.0f;
};

struct FAnimNode_StateMachine : public FAnimNode_Base
{
public:
	bool AddState(const FName& StateName, FAnimNode_Base* StateNode);
	bool SetState(const FName& StateName, bool bResetPlayback = true);
	FName GetCurrentStateName() const;
	bool IsCurrentStateFinished() const;
	bool AddTransition(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName = NAME_None, int32 Priority = 0, float BlendDuration = 0.0f, float FloatThreshold = 0.0f);
	bool AddAnyStateTransition(const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName = NAME_None, int32 Priority = 0, float BlendDuration = 0.0f, float FloatThreshold = 0.0f);
	void ClearTransitions();
	bool HasState(const FName& StateName) const;
	int32 GetStateCount() const { return States.Num(); }
	FName GetStateName(int32 Index) const { return States.IsValidIndex(Index) ? States[Index].Name : NAME_None; }
	bool IsBlending() const { return bIsBlending; }
	float GetTransitionAlpha() const { return bIsBlending ? TransitionBlendNode.GetAlpha() : 1.0f; }
	void GetDebugInfo(FAnimStateMachineDebugInfo& OutInfo) const;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual bool EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const override;
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
	bool AddTransitionInternal(const FName& FromState, const FName& ToState, EAnimTransitionRule Rule, const FName& ParameterName, int32 Priority, float BlendDuration, float FloatThreshold, bool bAnyState);
	void EvaluateTransitions(const FAnimationUpdateContext& Context);
	bool IsTransitionRuleMet(const FAnimStateTransition& Transition, const FAnimationUpdateContext& Context) const;
	FAnimNode_TwoWayBlend TransitionBlendNode;
	float TransitionBlendDuration = 0.0f;
	float TransitionBlendElapsed = 0.0f;
	bool bIsBlending = false;
	TArray<FAnimStateEntry> States;
	int32 CurrentStateIndex = -1;
	TArray<FAnimStateTransition> Transitions;
	FAnimStateTransition LastTransition;
	bool bHasLastTransition = false;
};