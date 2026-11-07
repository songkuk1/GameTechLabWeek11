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
	//TwoWayBlendNode* ConnectBlendNode(TwoWayBlendNode Blend, SequencePlayerNode A, SequencePlayerNode B);
	void RemoveNode(AnimNode* Node);
	void SetRoot(AnimNode* Node);

protected:
	TArray<TUniquePtr<AnimNode>> Nodes;
	AnimNode* Root = nullptr;
	float Speed;
	float CurrentTime = 0;
	bool bRunning = false;
};
