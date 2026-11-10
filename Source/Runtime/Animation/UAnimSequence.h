#pragma once

#include "EnginePCH.h"
#include "UAnimationAsset.h"
#include "AnimationData.h"
#include "PoseContext.h"

struct NotifyData
{
	FString NotifyName;
	float TriggerTime;
	float Duration;
};

class UAnimSequence : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequence,UAnimationAsset)
public:
	FAnimSeqData GetAnimData() const { return AnimData; }
	float GetPlayLength() const { return AnimData.PlayLength; }
	float GetFrameRate() const { return AnimData.FrameRate; }
	TArray<FBoneAnimTrack> GetTracks() const { return AnimData.BoneAnimationTracks; }
	FBoneAnimTrack GetTrackByIndex(uint32 Index) { return AnimData.BoneAnimationTracks[Index]; }
	const TArray<NotifyData>& GetNotifyData() const { return NotifyArray; }

	void SetAnimData(FAnimSeqData InAnimData) { AnimData = InAnimData; }
	uint32 AddNotify(NotifyData Notify);
	bool SetNotifyTriggerTime(uint32 Index, float NewTime);
	bool UpdateNotify(uint32 Index, const NotifyData& Notify);
	bool RemoveNotify(uint32 Index);

	void CalculateCurrentPose(float CurrentTime, bool bLoop, TArray<PoseTransform>& OutPoseTrans);

	FTransform CalculateRootTransform(float PreviousTime, float CurrentTime, bool bLoop);

	bool bEnableRootMotion;
private:
	FAnimSeqData AnimData;
	TArray<NotifyData> NotifyArray;
	
};
