#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "PoseContext.h"

class UAnimationInstance : public UObject
{
	DECLARE_CLASS(UAnimationInstance, UObject)
public:

	UAnimationInstance();
	~UAnimationInstance();

	void Tick(float deltaTime, PoseContext& Context);

protected:
	AnimNode* Root = nullptr;
};
