#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include <algorithm>

class UAnimSingleNodeInstance : public UAnimInstance
{
	DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
	REFLECT_START(UAnimSingleNodeInstance)
	REFLECT_END()

public:
	UAnimSingleNodeInstance() = default;
	virtual ~UAnimSingleNodeInstance() override = default;

	virtual void UpdateAnimation(float DeltaSeconds) override;

	void SetAnimation(UAnimationAsset* InAsset);
	UAnimationAsset* GetAnimation() const { return CurrentAsset; }

	void Play();
	void PlayReverse();
	bool IsReverse() const { return GetPlayRate() < 0.0f; }
	void StepForward(float StepTime);
	void StepBackward(float StepTime);
	void Stop();
	void Pause() { bPlaying = false; }
	bool IsPlaying() const { return bPlaying; }

	float GetPosition() const { return GetCurrentTime(); }
	void SetPosition(float InTime);

protected:
	UAnimationAsset* CurrentAsset = nullptr;
	bool bPlaying = true;
};
