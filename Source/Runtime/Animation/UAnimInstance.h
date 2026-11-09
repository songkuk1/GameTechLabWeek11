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

	AnimNode* AddNode(NodeType Node);
	void RemoveNode(AnimNode* Node);
	void SetRoot(AnimNode* Node);
	void SetSpeed(float InSpeed) { Speed = InSpeed; }
	void RequestAttack() { IsAttacking = true; }
	void RequestJumping() { IsJumping = true; }
	void RequestHit() { IsHit = true; }

protected:
	TArray<TUniquePtr<AnimNode>> Nodes;
	AnimNode* Root = nullptr;
	float JumpTime = 0.0f;
	float Speed = 0.0f;
	float CurrentTime = 0;
	bool bRunning = false;
	bool IsAttacking = false;
	bool IsHit = false;
	bool IsJumping = false;
};
