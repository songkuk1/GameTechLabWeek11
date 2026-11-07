#pragma once

#include "EnginePCH.h"
#include "UAnimationAsset.h"
#include "AnimationData.h"

class UAnimSequence : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequence,UAnimationAsset)
public:
	FAnimSeqData GetAnimData() const { return AnimData; }
	TArray<FBoneAnimTrack> GetTracks() { return AnimData.BoneAnimationTracks; }
	FBoneAnimTrack GetTrackByIndex(uint32 Index) { return AnimData.BoneAnimationTracks[Index]; }

	void SetAnimData(FAnimSeqData InAnimData) { AnimData = InAnimData; }

	void CalculateCurrentPose(float CurrentTime, TArray<FMatrix>& OutPose, bool bLoop = false);

private:
	FAnimSeqData AnimData;
};
