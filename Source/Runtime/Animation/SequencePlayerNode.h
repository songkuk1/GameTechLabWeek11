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

		if(bReverse) CurrentPlayTime -= AnimContext.deltaTime;
		else CurrentPlayTime += AnimContext.deltaTime;

		if (!Sequence)
			return;

		if(Sequence->bEnableRootMotion)
		{
			AnimContext.RootTransform = Sequence->CalculateRootTransform(PreviousTime, CurrentPlayTime, bLooping);
			AnimContext.RootMotionNodeIndex = Sequence->GetAnimData().RootNodeIndex;
		}

		const double PlayLength = Sequence->GetPlayLength();
		if (bLooping && PlayLength <= 0.0)
			return;

		// 비반복은 원래 시간으로, 반복은 각 바퀴의 누적 발생 시점으로 검사한다.
		const bool bMovingReverse = CurrentPlayTime < PreviousTime;
		const double FirstLoop = bLooping ? std::floor(PreviousTime / PlayLength) : 0.0;
		const double LastLoop = bLooping ? std::floor(CurrentPlayTime / PlayLength) : 0.0;
		const double LoopStep = bMovingReverse ? -1.0 : 1.0;
		for (double LoopIndex = FirstLoop;
			bMovingReverse ? LoopIndex >= LastLoop : LoopIndex <= LastLoop;
			LoopIndex += LoopStep)
		{
			for (const NotifyData& Notify : Sequence->GetNotifyData())
			{
				const double TriggerTime = Notify.TriggerTime + LoopIndex * PlayLength;
				const bool bCrossed = bMovingReverse
					? CurrentPlayTime <= TriggerTime && TriggerTime < PreviousTime
					: PreviousTime < TriggerTime && TriggerTime <= CurrentPlayTime;
				if (Notify.Duration == 0.0f && bCrossed)
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

	void InitializeNode(bool bLoop, bool InReverse, const FString& Path)
	{
		SetLoop(bLoop);
		SetSequence(UAssetManager::GetAssetByPath<UAnimSequence>(Path));
		bReverse = InReverse;
		if (bReverse) SetCurrentPlayTime(Sequence->GetPlayLength());
		else SetCurrentPlayTime(0);
	}

	void EvaluatePose(PoseContext& Context) override
	{
		Sequence->CalculateCurrentPose(CurrentPlayTime, bLooping, Context.LocalPoseTrans);
		if (Sequence->bEnableRootMotion)
		{
			for (const FBoneAnimTrack& BoneTrack : Sequence->GetAnimData().BoneAnimationTracks)
			{
				if (BoneTrack.InternalTrack.NodeIndex == static_cast<uint32>(Sequence->GetAnimData().RootNodeIndex))	// Root Lock
				{
					Context.LocalPoseTrans[BoneTrack.InternalTrack.NodeIndex].Translation = BoneTrack.InternalTrack.PosKeys[0];
					Context.LocalPoseTrans[BoneTrack.InternalTrack.NodeIndex].Rotation = BoneTrack.InternalTrack.RotKeys[0];
					Context.LocalPoseTrans[BoneTrack.InternalTrack.NodeIndex].Scale = BoneTrack.InternalTrack.ScaleKeys[0];
				}
			}
		}
	}

	bool IsClipFinished() override { if (bReverse) return CurrentPlayTime <= 0;
		else return CurrentPlayTime >= Sequence->GetAnimData().PlayLength; }

	void ResetAnimation() override {if (bReverse) SetCurrentPlayTime(Sequence->GetPlayLength()); else SetCurrentPlayTime(0); }

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
	bool bReverse = false;
};
