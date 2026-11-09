#pragma once

#include "Animation/AnimNode_Base.h"

class UAnimSequence;

struct FAnimNode_SequencePlayer : public FAnimNode_Base, public IAnimPlaybackControl
{
	UAnimSequence* Sequence = nullptr;
	float CurrentTime = 0.0f;
	float PlayRate = 1.0f;
	bool bLoop = true;
	bool bHasUpdated = false;
	bool bExtractRootMotion = false;
	void SetSequence(UAnimSequence* InSequence);
	void SetPlaybackPosition(float InTime) override;
	float GetCurrentTime() const override { return CurrentTime; }
	float GetPlayLength() const override;
	float GetFrameRate() const override;
	void SetPlayRate(float InPlayRate) override;
	float GetPlayRate() const override { return PlayRate; }
	void SetLooping(bool bInLoop) override { bLoop = bInLoop; }
	bool IsLooping() const override { return bLoop; }
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual bool EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const override;
	IAnimPlaybackControl* GetPlaybackControl() override { return this; }
	const IAnimPlaybackControl* GetPlaybackControl() const override { return this; }
};