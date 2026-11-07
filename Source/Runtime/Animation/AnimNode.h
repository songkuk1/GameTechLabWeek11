#pragma once

#include "EnginePCH.h"
#include "PoseContext.h"

class AnimNode
{
public:
	virtual void Update(float deltaTime) {};

	virtual void EvaluatePose(PoseContext& Context) {};
};
