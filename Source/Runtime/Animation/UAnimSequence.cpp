#include "EnginePCH.h"
#include "UAnimSequence.h"
#include <algorithm>
#include <cmath>

void UAnimSequence::CalculateCurrentPose(float CurrentTime, bool bLoop, TArray<PoseTransform>& OutPoseTrans)
{
	if (AnimData.NumberOfKeys <= 0 || !std::isfinite(CurrentTime)
		|| !std::isfinite(AnimData.FrameRate) || AnimData.FrameRate <= 0.0f
		|| !std::isfinite(AnimData.PlayLength) || AnimData.PlayLength < 0.0f)
		return;

	const int32 LastIndex = AnimData.NumberOfKeys - 1;	// 키프레임 전체 수
	float SampleTime = std::clamp(CurrentTime, 0.0f, AnimData.PlayLength);	// 현재 애니메이션 프레임 시간, 프레임 끝까지가면 마지막으로 유지
	if (bLoop && AnimData.PlayLength > 0.0f)
	{
		SampleTime = std::fmod(CurrentTime, AnimData.PlayLength);

		if (SampleTime < 0.0f)
			SampleTime += AnimData.PlayLength;
	}
	const double KeyFramePos = SampleTime >= AnimData.PlayLength? static_cast<double>(LastIndex) : std::clamp(static_cast<double>(SampleTime) * AnimData.FrameRate, 0.0, static_cast<double>(LastIndex));	// 키프레임 현재위치 = FrameRate * 현재 흐른 시간
	const int32 StartIndex = static_cast<int32>(std::floor(KeyFramePos));
	const int32 EndIndex = StartIndex < LastIndex ? StartIndex + 1 : LastIndex;  // 현재 구간 처음 인덱스
	const float alpha = static_cast<float>(KeyFramePos - StartIndex);	// 그 다음 인덱스

	for (int i = 0; i < AnimData.BoneAnimationTracks.Num();i++)
	{
		const auto& Track = AnimData.BoneAnimationTracks[i].InternalTrack;
		// OutPose는 호출자가 대상 Skeleton의 기본 로컬 자세로 준비한다.
		if (Track.NodeIndex >= static_cast<uint32>(OutPoseTrans.Num())
			|| Track.PosKeys.Num() < AnimData.NumberOfKeys
			|| Track.RotKeys.Num() < AnimData.NumberOfKeys
			|| Track.ScaleKeys.Num() < AnimData.NumberOfKeys)
			continue;

		float Xpos = std::lerp(AnimData.BoneAnimationTracks[i].InternalTrack.PosKeys[StartIndex].X, AnimData.BoneAnimationTracks[i].InternalTrack.PosKeys[EndIndex].X, alpha);
		float Ypos = std::lerp(AnimData.BoneAnimationTracks[i].InternalTrack.PosKeys[StartIndex].Y, AnimData.BoneAnimationTracks[i].InternalTrack.PosKeys[EndIndex].Y, alpha);
		float Zpos = std::lerp(AnimData.BoneAnimationTracks[i].InternalTrack.PosKeys[StartIndex].Z, AnimData.BoneAnimationTracks[i].InternalTrack.PosKeys[EndIndex].Z, alpha);
		FQuat xyzRot = FQuat::Slerp(AnimData.BoneAnimationTracks[i].InternalTrack.RotKeys[StartIndex],AnimData.BoneAnimationTracks[i].InternalTrack.RotKeys[EndIndex],alpha);
		float Xscale = std::lerp(AnimData.BoneAnimationTracks[i].InternalTrack.ScaleKeys[StartIndex].X, AnimData.BoneAnimationTracks[i].InternalTrack.ScaleKeys[EndIndex].X, alpha);
		float Yscale = std::lerp(AnimData.BoneAnimationTracks[i].InternalTrack.ScaleKeys[StartIndex].Y, AnimData.BoneAnimationTracks[i].InternalTrack.ScaleKeys[EndIndex].Y, alpha);
		float Zscale = std::lerp(AnimData.BoneAnimationTracks[i].InternalTrack.ScaleKeys[StartIndex].Z, AnimData.BoneAnimationTracks[i].InternalTrack.ScaleKeys[EndIndex].Z, alpha);
		FVector AnimPos = FVector(Xpos, Ypos, Zpos);
		FQuat AnimRot = xyzRot;
		FVector AnimScale = FVector(Xscale, Yscale, Zscale);
		PoseTransform PoseTrans;
		PoseTrans.Translation = AnimPos;
		PoseTrans.Rotation = AnimRot;
		PoseTrans.Scale = AnimScale;	// 가까운 Index 사이에서 Alpha Lerp 진행함
		OutPoseTrans[AnimData.BoneAnimationTracks[i].InternalTrack.NodeIndex] = PoseTrans;
	}
}

uint32 UAnimSequence::AddNotify(NotifyData Notify)
{
	uint32 NotifyIndex = NotifyArray.Num();
	NotifyArray.Add(Notify);
	return NotifyIndex;
		
}

bool UAnimSequence::SetNotifyTriggerTime(uint32 Index, float NewTime)
{
	if (Index >= static_cast<uint32>(NotifyArray.Num()))
		return false;

	NotifyData Updated = NotifyArray[Index];
	Updated.TriggerTime = NewTime;
	return UpdateNotify(Index, Updated);
}

bool UAnimSequence::UpdateNotify(uint32 Index, const NotifyData& Notify)
{
	if (Index >= static_cast<uint32>(NotifyArray.Num()) || !std::isfinite(Notify.TriggerTime)
		|| !std::isfinite(Notify.Duration) || !std::isfinite(AnimData.PlayLength)
		|| AnimData.PlayLength < 0.0f || Notify.TriggerTime < 0.0f
		|| Notify.TriggerTime > AnimData.PlayLength || Notify.Duration < 0.0f
		|| Notify.Duration > AnimData.PlayLength - Notify.TriggerTime)
		return false;

	NotifyArray[Index] = Notify;
	return true;
}

bool UAnimSequence::RemoveNotify(uint32 Index)
{
	if (Index >= static_cast<uint32>(NotifyArray.Num()))
		return false;

	NotifyArray.RemoveAt(Index);
	return true;
}
