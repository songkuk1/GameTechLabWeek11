#pragma once

#include "AnimNode.h"
#include <functional>
#include <utility>

enum class EAnimState : uint8
{
	Locomotion,   // 안에 BlendSpace1D (Idle / Walk / Run )
	Attack,
	Hit,
	JumpStart,
	JumpAir,
	JumpEnd
};

struct Transition
{
	EAnimState StartState;
	EAnimState EndState;
	std::function<bool(const AnimationContext&)> Condition;
	float BlendValue;
	std::function<void(AnimationContext&)> OnTransition;
};

class StateMachineNode : public AnimNode
{
	EAnimState State = EAnimState::Locomotion;
	EAnimState NextState = EAnimState::Locomotion;
	AnimNode* StateNodes[6] = {};        // 상태마다 포즈를 내는 노드
	TArray<Transition> TransitionArray[6];

public:
	StateMachineNode() = default;

	void SetStateNode(EAnimState InState, AnimNode* Node) { StateNodes[(int)InState] = Node; }

	void Update(AnimationContext& AnimContext) override
	{
		ProcessState(AnimContext);
		StateNodes[(int)State]->Update(AnimContext);   // 현재 상태 노드에 전파
	}

	void EvaluatePose(PoseContext& Context) override
	{
		StateNodes[(int)State]->EvaluatePose(Context);  // 현재 상태 노드의 포즈를 그대로 쓴다
	}

	void AddTransition(EAnimState StartState, EAnimState EndState,
		std::function<bool(const AnimationContext&)> Condition, float BlendAlpha,
		std::function<void(AnimationContext&)> OnTransition = {})
	{
		Transition Trans{ StartState, EndState, std::move(Condition), BlendAlpha, std::move(OnTransition) };
		TransitionArray[(int)StartState].Add(std::move(Trans));
	}

private:
	void ProcessState(AnimationContext& AnimContext)
	{
		const auto& Transitions = TransitionArray[(int)State];

		for (const Transition& Trans : Transitions)
		{
			if (Trans.Condition(AnimContext))
			{
				State = Trans.EndState;
				if (Trans.OnTransition)
					Trans.OnTransition(AnimContext);
				break;
			}
		}
	}
};
