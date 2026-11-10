#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "PoseContext.h"
#include "UObject/Class.h"

class UAnimationInstance : public UObject
{
	DECLARE_CLASS(UAnimationInstance, UObject)
public:

	UAnimationInstance();
	~UAnimationInstance();

	void Tick(float deltaTime, PoseContext& Context);

	FTransform GetRootTransform() const { return RootTransform; }
	int32 GetRootMotionNodeIndex() const { return RootMotionNodeIndex; }

	AnimNode* AddNode(NodeType Node);
	void RemoveNode(AnimNode* Node);
	void SetRoot(AnimNode* Node);
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
