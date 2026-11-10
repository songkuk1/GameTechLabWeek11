#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "PoseContext.h"
#include "UObject/Class.h"

class SequencePlayerNode;
class TwoWayBlendNode;
class BlendSpace1DNode;
class StateMachineNode;
class UAnimSequence;

class UAnimationInstance : public UObject
{
	DECLARE_CLASS(UAnimationInstance, UObject)
public:

	UAnimationInstance();
	~UAnimationInstance();

	void Tick(float deltaTime, PoseContext& Context);

	// 그래프 초기화 및 프리셋 빌드
	void ClearGraph();
	void BuildDefaultLocomotionGraph();

	// 타입별 노드 생성 빌더 API
	AnimNode* AddNode(NodeType Node);
	SequencePlayerNode* AddSequenceNode(const FString& Path, bool bLoop = true, bool bReverse = false);
	SequencePlayerNode* AddSequenceNode(UAnimSequence* InSequence, bool bLoop = true, bool bReverse = false);
	TwoWayBlendNode* AddTwoWayBlendNode(AnimNode* NodeA = nullptr, AnimNode* NodeB = nullptr, float Alpha = 0.0f);
	BlendSpace1DNode* AddBlendSpace1DNode();
	StateMachineNode* AddStateMachineNode();

	// 단일 클립 재생용 헬퍼 (기존 그래프를 비우고 단일 시퀀스 플레이어를 루트로 설정)
	SequencePlayerNode* PlaySingleAnimation(const FString& Path, bool bLoop = true, bool bReverse = false);
	SequencePlayerNode* PlaySingleAnimation(UAnimSequence* InSequence, bool bLoop = true, bool bReverse = false);

	void RemoveNode(AnimNode* Node);
	void SetRoot(AnimNode* Node);
	AnimNode* GetRoot() const { return Root; }

	FTransform GetRootTransform() const { return RootTransform; }
	int32 GetRootMotionNodeIndex() const { return RootMotionNodeIndex; }

	void SetSpeed(float InSpeed) { Speed = InSpeed; }
	void RequestAttack() { IsAttacking = true; }
	void RequestJumping() { IsJumpStart = true; }
	void SetJumpConditions(bool bStart, bool bAir, bool bEnd)
	{
		IsJumpStart = bStart;
		IsJumpAir = bAir;
		IsJumpEnd = bEnd;
	}
	void RequestHit() { IsHit = true; }

	TArray<FString> PendingNotifyNames;

protected:
	TArray<TUniquePtr<AnimNode>> Nodes;
	FTransform RootTransform = FTransform::Identity;
	int32 RootMotionNodeIndex = -1;
	AnimNode* Root = nullptr;
	float JumpTime = 0.0f;
	float Speed = 0.0f;
	float CurrentTime = 0;
	bool bRunning = false;
	bool IsAttacking = false;
	bool IsHit = false;
	bool IsJumpStart = false;
	bool IsJumpAir = false;
	bool IsJumpEnd = false;
};
