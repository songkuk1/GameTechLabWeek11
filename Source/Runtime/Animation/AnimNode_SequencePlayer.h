#pragma once

#include "Animation/AnimNode_Base.h"

class UAnimSequence;

struct FAnimNode_SequencePlayer : public FAnimNode_Base
{
	UAnimSequence* Sequence = nullptr;
	float CurrentTime = 0.0f;
	float PlayRate = 0.0f;
	bool bLoop = true;
	void SetSequence(UAnimSequence* InSequence);
	void SetPlaybackPosition(float InTime);
	float GetCurrentTime() const { return CurrentTime; }
	float GetPlayLength() const;
	float GetFrameRate() const;
	void SetPlayRate(float InPlayRate);
	float GetPlayRate() const { return PlayRate; }
	void SetLooping(bool bInLoop) { bLoop = bInLoop; }
	bool IsLooping() const { return bLoop; }
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
};