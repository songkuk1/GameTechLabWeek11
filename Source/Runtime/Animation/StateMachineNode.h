#pragma once

#include "AnimNode.h"
#include "Containers/Map.h"
#include "Core/NameTypes.h"
#include <functional>
#include <utility>

struct Transition
{
	FName StartState;
	FName EndState;
	std::function<bool(const AnimationContext&)> Condition;
	std::function<void(AnimationContext&)> OnTransition;
	float BlendValue = 0.0f;
};

class StateMachineNode : public AnimNode
{
	FName CurrentState = "Locomotion";
	TMap<FName, AnimNode*> StateNodes;
	TMap<FName, TArray<Transition>> TransitionMap;
	TArray<Transition> AnyStateTransitions;

public:
	StateMachineNode() = default;

	void SetInitialState(const FName& InState) { CurrentState = InState; }

	const FName& GetCurrentState() const { return CurrentState; }

	void SetStateNode(const FName& InState, AnimNode* Node)
	{
		if (StateNodes.IsEmpty())
		{
			CurrentState = InState;
		}
		StateNodes.Add(InState, Node);
	}

	AnimNode* GetStateNode(const FName& InState) const
	{
		return StateNodes.FindRef(InState);
	}

	void Update(AnimationContext& AnimContext) override
	{
		ProcessState(AnimContext);
		if (AnimNode* ActiveNode = StateNodes.FindRef(CurrentState))
		{
			ActiveNode->Update(AnimContext);
		}
	}

	void EvaluatePose(PoseContext& Context) override
	{
		if (AnimNode* ActiveNode = StateNodes.FindRef(CurrentState))
		{
			ActiveNode->EvaluatePose(Context);
		}
	}

	void AddTransition(
		const FName& StartState,
		const FName& EndState,
		std::function<bool(const AnimationContext&)> Condition,
		float BlendAlpha = 0.0f,
		std::function<void(AnimationContext&)> OnTransition = {})
	{
		Transition Trans{ StartState, EndState, std::move(Condition), std::move(OnTransition), BlendAlpha };
		TransitionMap[StartState].Add(std::move(Trans));
	}

	void AddAnyStateTransition(
		const FName& EndState,
		std::function<bool(const AnimationContext&)> Condition,
		float BlendAlpha = 0.0f,
		std::function<void(AnimationContext&)> OnTransition = {})
	{
		Transition Trans{ NAME_None, EndState, std::move(Condition), std::move(OnTransition), BlendAlpha };
		AnyStateTransitions.Add(std::move(Trans));
	}

private:
	void ProcessState(AnimationContext& AnimContext)
	{
		for (const Transition& Trans : AnyStateTransitions)
		{
			if (Trans.Condition && Trans.Condition(AnimContext))
			{
				CurrentState = Trans.EndState;
				if (Trans.OnTransition)
					Trans.OnTransition(AnimContext);
				return;
			}
		}

		const TArray<Transition>* Transitions = TransitionMap.Find(CurrentState);
		if (!Transitions)
			return;

		for (const Transition& Trans : *Transitions)
		{
			if (Trans.Condition && Trans.Condition(AnimContext))
			{
				CurrentState = Trans.EndState;
				if (Trans.OnTransition)
					Trans.OnTransition(AnimContext);
				break;
			}
		}
	}
};
