#pragma once

#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_TwoWayBlend.h"

class UBlendSpace1D;

struct FAnimNode_BlendSpacePlayer : public FAnimNode_Base, public IAnimPlaybackControl
{
public:
    FAnimNode_BlendSpacePlayer();
    void SetBlendSpace(UBlendSpace1D* InBlendSpace);
    void SetBlendInput(float InInput);
    float GetBlendInput() const { return BlendInput; }
	void SetPlayRate(float InPlayRate) override;
	float GetPlayRate() const override { return PlayRate; }
	void SetLooping(bool bInLoop) override;
	bool IsLooping() const override { return bLoop; }
	void SetPlaybackPosition(float InTime) override;
	float GetCurrentTime() const override;
	float GetPlayLength() const override;
	float GetFrameRate() const override;
    virtual void Update(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate(FPoseContext& Output) override;
	IAnimPlaybackControl* GetPlaybackControl() override { return this; }
	const IAnimPlaybackControl* GetPlaybackControl() const override { return this; }
private:
	UBlendSpace1D* BlendSpace = nullptr;
	float BlendInput = 0.0f;
	float PlayRate = 1.0f;
	float NormalizedTime = 0.0f;
	bool bLoop = true;
	int32 PreviousSampleA = -1;
	int32 PreviousSampleB = -1;
	FAnimNode_SequencePlayer PlayerA;
	FAnimNode_SequencePlayer PlayerB;
	FAnimNode_TwoWayBlend BlendNode;
};