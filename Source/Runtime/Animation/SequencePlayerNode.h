#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "UAnimSequence.h"
#include "PoseContext.h"
#include "Asset/AssetManager.h"
#include <cmath>

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
		const float PreviousTime = CurrentPlayTime;
		CurrentPlayTime += AnimContext.deltaTime;
		if (!Sequence)
			return;

		const double PlayLength = Sequence->GetPlayLength();
		if (bLooping && PlayLength <= 0.0)
			return;

		// 비반복은 원래 시간으로, 반복은 각 바퀴의 누적 발생 시점으로 검사한다.
		const double FirstLoop = bLooping ? std::floor(PreviousTime / PlayLength) : 0.0;
		const double LastLoop = bLooping ? std::floor(CurrentPlayTime / PlayLength) : 0.0;
		for (double LoopIndex = FirstLoop; LoopIndex <= LastLoop; LoopIndex += 1.0)
		{
			for (const NotifyData& Notify : Sequence->GetNotifyData())
			{
				const double TriggerTime = Notify.TriggerTime + LoopIndex * PlayLength;
				if (Notify.Duration == 0.0f && PreviousTime < TriggerTime && TriggerTime <= CurrentPlayTime)
					AnimContext.TriggeredNotifyNames.Add(Notify.NotifyName);
			}
		}
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
		if (Sequence->GetNotifyData()[Index].TriggerTime < CurrentPlayTime && CurrentPlayTime <= Sequence->GetNotifyData()[Index].TriggerTime+ Sequence->GetNotifyData()[Index].Duration)
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
