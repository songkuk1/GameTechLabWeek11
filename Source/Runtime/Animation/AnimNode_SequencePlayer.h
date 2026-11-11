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
	bool bAllowRootMotion = true;
	bool bExtractRootMotion = false;
	// 루트 모션이 켜져 있으면 이 플레이어가 루트 모션을 액터에 주지 않더라도(bAllowRootMotion == false)
	// 포즈의 루트 이동은 지운다 (루트 락). 무브먼트 컴포넌트가 움직이는 로코모션에서 메시가 캡슐보다 앞서 나가지 않게 한다.
	bool bLockRoot = false;
	float CurrentRootMotionScale = 1.0f;
	void SetSequence(UAnimSequence* InSequence);
	void SetAllowRootMotion(bool bInAllow) { bAllowRootMotion = bInAllow; }
	bool IsAllowingRootMotion() const { return bAllowRootMotion; }
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