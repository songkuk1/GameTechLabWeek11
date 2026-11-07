#pragma once

#include "AnimNode.h"

enum class EAnimState : uint8
{
	Locomotion,   // 안에 BlendSpace1D (Idle / Walk / Run)
	Attack,
	Hit,
};

class StateMachineNode : public AnimNode
{
	EAnimState State = EAnimState::Locomotion;
	AnimNode* StateNodes[3];        // 상태마다 포즈를 내는 노드

	void SetStateNode(EAnimState InState, AnimNode* Node) { StateNodes[(int)InState] = Node; }

	void Update(PoseContext Context) override
	{
		UMyAnimInstance* AnimInstance = Context.GetAnimInstance<UMyAnimInstance>(); // 단순화 (UE: AnimInstanceProxy)
		ProcessState(AnimInstance);
		StateNodes[(int)State]->Update(Context);   // 현재 상태 노드에 전파
	}

	virtual void Evaluate(FPoseContext& Output) override
	{
		StateNodes[(int)State]->Evaluate(Output);  // 현재 상태 노드의 포즈를 그대로 쓴다
	}

	void ProcessState(UMyAnimInstance* AnimInstance)
	{
		if (AnimInstance->bIsHit)                          // Any State: 어디서든 Hit 이 끊고 들어온다
		{
			State = EAnimState::Hit;
			AnimInstance->bIsHit = false;
		}
		else if (State == EAnimState::Locomotion && AnimInstance->bIsAttacking)
		{
			State = EAnimState::Attack;
			AnimInstance->bIsAttacking = false;
		}
		else if (State != EAnimState::Locomotion && IsClipFinished())   // UE: Automatic Rule / Time Remaining
		{
			State = EAnimState::Locomotion;
		}
	}
};
