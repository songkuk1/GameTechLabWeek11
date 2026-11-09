#pragma once

#include "EnginePCH.h"
#include "PoseContext.h"

enum NodeType
{
	SequencePlayer,
	TwoWayBlend,
	BlendSpace1D,
	StateMachine
};

class AnimNode
{
public:
	virtual ~AnimNode() = default;

	virtual void Update(AnimationContext& AnimContext) {};

	virtual void EvaluatePose(PoseContext& Context) {};

	virtual bool IsClipFinished() { return false; }

	virtual void ResetAnimation() {};
};

