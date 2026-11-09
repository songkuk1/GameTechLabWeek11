#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "UAnimSequence.h"
#include "PoseContext.h"
#include "Asset/AssetManager.h"

class SequencePlayerNode : public AnimNode
{
public:
	SequencePlayerNode()
	{

	}

	~SequencePlayerNode()
	{
		Sequence = nullptr;
	}

	void Update(AnimationContext& AnimContext) override
	{
		CurrentPlayTime += AnimContext.deltaTime;
	}

	float GetCurrentPlayTime() const { return CurrentPlayTime; }
	UAnimSequence* GetSequence() const { return Sequence; }
	bool IsLoop() const { return bLooping; }

	void SetCurrentPlayTime(float Time) { CurrentPlayTime = Time; }
	void SetSequence(UAnimSequence* AnimSeq) { Sequence = AnimSeq; }
	void SetLoop(bool InLoop) { bLooping = InLoop; }

	void InitializeNode(bool bLoop, const FString& Path)
	{
		SetCurrentPlayTime(0);
		SetLoop(bLoop);
		SetSequence(UAssetManager::GetAssetByPath<UAnimSequence>(Path));
	}

	void EvaluatePose(PoseContext& Context) override
	{
		Sequence->CalculateCurrentPose(CurrentPlayTime, bLooping, Context.LocalPoseTrans);
	}

	bool IsClipFinished() override { return CurrentPlayTime >= Sequence->GetAnimData().PlayLength; }

	void ResetAnimation() override { SetCurrentPlayTime(0); }

	bool IsNotifyActivated(uint32 Index)
	{
		if (Sequence->GetNotifyData()[Index].TriggerTime <= CurrentPlayTime && CurrentPlayTime <= Sequence->GetNotifyData()[Index].TriggerTime+ Sequence->GetNotifyData()[Index].Duration)
		{
			return true;
		}
		else return false;
	}

private:
	UAnimSequence* Sequence = nullptr;
	float CurrentPlayTime = 0;
	bool bLooping = false;
};
