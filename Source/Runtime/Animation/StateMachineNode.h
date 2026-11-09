#pragma once

#include "AnimNode.h"

enum class EAnimState : uint8
{
	Locomotion,   // 안에 BlendSpace1D (Idle / Walk / Run )
	Attack,
	Hit,
	Jump
};

class StateMachineNode : public AnimNode
{
	EAnimState State = EAnimState::Locomotion;
	AnimNode* StateNodes[4] = {};        // 상태마다 포즈를 내는 노드
	bool bIsCilpFinshed = false;

public:
	StateMachineNode() = default;

	void SetStateNode(EAnimState InState, AnimNode* Node) { StateNodes[(int)InState] = Node; }

	void Update(AnimationContext& AnimContext) override
	{
		ProcessState(AnimContext);
		StateNodes[(int)State]->Update(AnimContext);   // 현재 상태 노드에 전파
		if (State == EAnimState::Hit || State == EAnimState::Attack) { bIsCilpFinshed = StateNodes[(int)State]->IsClipFinished(); }
	}

	void EvaluatePose(PoseContext& Context) override
	{
		StateNodes[(int)State]->EvaluatePose(Context);  // 현재 상태 노드의 포즈를 그대로 쓴다
	}

private:
	void ProcessState(AnimationContext& AnimContext)
	{
		if (AnimContext.bIsHit)                          // Any State: 어디서든 Hit 이 끊고 들어온다
		{
			State = EAnimState::Hit;
			AnimContext.bIsHit = false;
			StateNodes[(int)State]->ResetAnimation();
		}
		else if (State == EAnimState::Locomotion && AnimContext.bIsAttacking)
		{
			State = EAnimState::Attack;
			AnimContext.bIsAttacking = false;
			StateNodes[(int)State]->ResetAnimation();
		}
		else if (State != EAnimState::Locomotion && bIsCilpFinshed)   // UE: Automatic Rule / Time Remaining
		{
			State = EAnimState::Locomotion;
		}
	}
};
