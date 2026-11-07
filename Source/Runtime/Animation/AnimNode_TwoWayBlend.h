#pragma once

#include "Animation/AnimNode_SequencePlayer.h"

struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
public: 
	void SetSourceNodes(FAnimNode_Base* InSourceA, FAnimNode_Base* InSourceB);
	void SetAlpha(float InAlpha);
	float GetAlpha() const { return Alpha; }
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
private:
	FAnimNode_Base* SourcePoseA = nullptr;
	FAnimNode_Base* SourcePoseB = nullptr;
	float Alpha = 0.5f;
};

struct FAnimNode_TwoWayBlendPlayer : public FAnimNode_Base, public IAnimPlaybackControl
{
public:
    FAnimNode_TwoWayBlendPlayer();
    void SetAnimations(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB);
    void SetAlpha(float InAlpha);
    float GetAlpha() const;
    void SetPlaybackPosition(float InTime) override;
    float GetCurrentTime() const override;
    float GetPlayLength() const override;
    float GetFrameRate() const override;
    void SetPlayRate(float InPlayRate) override;
    float GetPlayRate() const override { return PlayRate; }
    void SetLooping(bool bInLoop) override;
    bool IsLooping() const override;
    IAnimPlaybackControl* GetPlaybackControl() override { return this; }
    const IAnimPlaybackControl* GetPlaybackControl() const override { return this; }
    virtual void Update(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate(FPoseContext& Output) override;

private:
	FAnimNode_SequencePlayer SequencePlayerA;
	FAnimNode_SequencePlayer SequencePlayerB;
	FAnimNode_TwoWayBlend BlendNode;
	float PlayRate = 1.0f;
};