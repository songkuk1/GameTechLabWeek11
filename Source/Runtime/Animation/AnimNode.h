#pragma once

#include "EnginePCH.h"
#include "PoseContext.h"

enum NodeType
{
	SequencePlayer,
	TwoWayBlend,
	BlendSpace1D
};

class AnimNode
{
public:
	virtual void Update(float deltaTime) {};

	virtual void EvaluatePose(PoseContext& Context) {};
};
