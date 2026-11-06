#pragma once

#include "Animation/AnimNode_Base.h"

class UAnimSequence;

struct FAnimNode_SequencePlayer : public FAnimNode_Base
{
	UAnimSequence* Sequence = nullptr;
	float CurrentTime = 0.0f;
	bool bLoop = true;
	void SetSequence(UAnimSequence* InSequence);
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
};