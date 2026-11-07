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

	void Update(float deltaTime) override
	{
		CurrentPlayTime += deltaTime;
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

private:
	UAnimSequence* Sequence = nullptr;
	float CurrentPlayTime = 0;
	bool bLooping = false;
};
