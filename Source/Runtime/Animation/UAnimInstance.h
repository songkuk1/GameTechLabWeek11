#pragma once

#include "EnginePCH.h"
#include "UAnimSequence.h"

class UAnimationInstance : public UObject
{
	DECLARE_CLASS(UAnimationInstance, UObject)
public:

	UAnimationInstance();
	~UAnimationInstance();

	void Tick(float deltaTime, TArray<FMatrix>& LocalPose);
	void UpdatePose(float deltaTime, TArray<FMatrix>& LocalPose);

	float GetCurrentPlayTime() const { return CurrentPlayTime; }
	UAnimSequence* GetSequence() const { return Sequence; }

	void SetCurrentPlayTime(float Time) { CurrentPlayTime = Time; }
	void SetSequence(UAnimSequence* AnimSeq) { Sequence = AnimSeq; }
	void SetLoop(bool InLoop) { bLooping = InLoop; }

private:
	float CurrentPlayTime; 
	UAnimSequence* Sequence;
	bool bLooping;
};