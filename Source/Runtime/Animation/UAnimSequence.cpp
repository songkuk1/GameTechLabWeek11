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


FTransform UAnimSequence::CalculateRootTransform(float PreviousTime, float CurrentTime, bool bLoop)
{
	if (AnimData.NumberOfKeys <= 0 || !std::isfinite(PreviousTime) || !std::isfinite(CurrentTime)
		|| !std::isfinite(AnimData.FrameRate) || AnimData.FrameRate <= 0.0f
		|| !std::isfinite(AnimData.PlayLength) || AnimData.PlayLength < 0.0f
		|| AnimData.RootNodeIndex < 0)
		return FTransform::Identity;

	const FAnimSeqTrack* RootTrack = nullptr;
	for (const FBoneAnimTrack& BoneTrack : AnimData.BoneAnimationTracks)
	{
		if (BoneTrack.InternalTrack.NodeIndex == static_cast<uint32>(AnimData.RootNodeIndex))
		{
			RootTrack = &BoneTrack.InternalTrack;
			break;
		}
	}
	if (!RootTrack || RootTrack->PosKeys.Num() < AnimData.NumberOfKeys
		|| RootTrack->RotKeys.Num() < AnimData.NumberOfKeys
		|| RootTrack->ScaleKeys.Num() < AnimData.NumberOfKeys)
		return FTransform::Identity;

	const int32 LastIndex = AnimData.NumberOfKeys - 1;

	// 이전 시간의 위치 / 회전 / 스케일 샘플링
	float PreviousSampleTime = std::clamp(PreviousTime, 0.0f, AnimData.PlayLength);
	double PreviousLoopIndex = 0.0;
	if (bLoop && AnimData.PlayLength > 0.0f)
	{
		PreviousLoopIndex = std::floor(static_cast<double>(PreviousTime) / AnimData.PlayLength);
		PreviousSampleTime = std::fmod(PreviousTime, AnimData.PlayLength);
		if (PreviousSampleTime < 0.0f)
			PreviousSampleTime += AnimData.PlayLength;
	}
	const double PreviousKeyFramePos = PreviousSampleTime >= AnimData.PlayLength
		? static_cast<double>(LastIndex)
		: std::clamp(static_cast<double>(PreviousSampleTime) * AnimData.FrameRate, 0.0, static_cast<double>(LastIndex));
	const int32 PreviousStartIndex = static_cast<int32>(std::floor(PreviousKeyFramePos));
	const int32 PreviousEndIndex = PreviousStartIndex < LastIndex ? PreviousStartIndex + 1 : LastIndex;
	const float PreviousAlpha = static_cast<float>(PreviousKeyFramePos - PreviousStartIndex);
	const FVector PreviousPosition(
		std::lerp(RootTrack->PosKeys[PreviousStartIndex].X, RootTrack->PosKeys[PreviousEndIndex].X, PreviousAlpha),
		std::lerp(RootTrack->PosKeys[PreviousStartIndex].Y, RootTrack->PosKeys[PreviousEndIndex].Y, PreviousAlpha),
		std::lerp(RootTrack->PosKeys[PreviousStartIndex].Z, RootTrack->PosKeys[PreviousEndIndex].Z, PreviousAlpha));
	const FQuat PreviousRotation = FQuat::Slerp(
		RootTrack->RotKeys[PreviousStartIndex], RootTrack->RotKeys[PreviousEndIndex], PreviousAlpha);
	const FVector PreviousScale(
		std::lerp(RootTrack->ScaleKeys[PreviousStartIndex].X, RootTrack->ScaleKeys[PreviousEndIndex].X, PreviousAlpha),
		std::lerp(RootTrack->ScaleKeys[PreviousStartIndex].Y, RootTrack->ScaleKeys[PreviousEndIndex].Y, PreviousAlpha),
		std::lerp(RootTrack->ScaleKeys[PreviousStartIndex].Z, RootTrack->ScaleKeys[PreviousEndIndex].Z, PreviousAlpha));

	// 현재 시간의 위치 / 회전 / 스케일 샘플링 (기존 SampleTime / StartIndex / EndIndex 계산)
	float SampleTime = std::clamp(CurrentTime, 0.0f, AnimData.PlayLength);
	double CurrentLoopIndex = 0.0;
	if (bLoop && AnimData.PlayLength > 0.0f)
	{
		CurrentLoopIndex = std::floor(static_cast<double>(CurrentTime) / AnimData.PlayLength);
		SampleTime = std::fmod(CurrentTime, AnimData.PlayLength);
		if (SampleTime < 0.0f)
			SampleTime += AnimData.PlayLength;
	}
	const double KeyFramePos = SampleTime >= AnimData.PlayLength
		? static_cast<double>(LastIndex)
		: std::clamp(static_cast<double>(SampleTime) * AnimData.FrameRate, 0.0, static_cast<double>(LastIndex));
	const int32 StartIndex = static_cast<int32>(std::floor(KeyFramePos));
	const int32 EndIndex = StartIndex < LastIndex ? StartIndex + 1 : LastIndex;
	const float alpha = static_cast<float>(KeyFramePos - StartIndex);
	const FVector CurrentPosition(
		std::lerp(RootTrack->PosKeys[StartIndex].X, RootTrack->PosKeys[EndIndex].X, alpha),
		std::lerp(RootTrack->PosKeys[StartIndex].Y, RootTrack->PosKeys[EndIndex].Y, alpha),
		std::lerp(RootTrack->PosKeys[StartIndex].Z, RootTrack->PosKeys[EndIndex].Z, alpha));
	const FQuat CurrentRotation = FQuat::Slerp(
		RootTrack->RotKeys[StartIndex], RootTrack->RotKeys[EndIndex], alpha);
	const FVector CurrentScale(
		std::lerp(RootTrack->ScaleKeys[StartIndex].X, RootTrack->ScaleKeys[EndIndex].X, alpha),
		std::lerp(RootTrack->ScaleKeys[StartIndex].Y, RootTrack->ScaleKeys[EndIndex].Y, alpha),
		std::lerp(RootTrack->ScaleKeys[StartIndex].Z, RootTrack->ScaleKeys[EndIndex].Z, alpha));

	if (std::abs(PreviousScale.X) <= SMALL_NUMBER || std::abs(PreviousScale.Y) <= SMALL_NUMBER
		|| std::abs(PreviousScale.Z) <= SMALL_NUMBER)
		return FTransform::Identity;

	const FTransform PreviousTransform(PreviousPosition, PreviousScale, PreviousRotation.ToFRotator());
	const FTransform CurrentTransform(CurrentPosition, CurrentScale, CurrentRotation.ToFRotator());
	const FMatrix PreviousMatrix = PreviousTransform.GetLocalMatrix();
	const FMatrix CurrentMatrix = CurrentTransform.GetLocalMatrix();

	// 반복 경계에서는 위치를 더하는 대신 클립 전체의 변환을 합성한다.
	FMatrix LoopCorrection = FMatrix::Identity;
	double LoopCount = CurrentLoopIndex - PreviousLoopIndex;
	if (LoopCount != 0.0)
	{
		const FVector& FirstScale = RootTrack->ScaleKeys[0];
		const FVector& LastScale = RootTrack->ScaleKeys[LastIndex];
		if (std::abs(FirstScale.X) <= SMALL_NUMBER || std::abs(FirstScale.Y) <= SMALL_NUMBER
			|| std::abs(FirstScale.Z) <= SMALL_NUMBER || std::abs(LastScale.X) <= SMALL_NUMBER
			|| std::abs(LastScale.Y) <= SMALL_NUMBER || std::abs(LastScale.Z) <= SMALL_NUMBER)
			return FTransform::Identity;

		const FTransform FirstTransform(RootTrack->PosKeys[0], FirstScale, RootTrack->RotKeys[0].ToFRotator());
		const FTransform LastTransform(RootTrack->PosKeys[LastIndex], LastScale, RootTrack->RotKeys[LastIndex].ToFRotator());
		FMatrix LoopMatrix = FirstTransform.GetLocalMatrix().Inverse() * LastTransform.GetLocalMatrix();
		if (LoopCount < 0.0)
			LoopMatrix = LoopMatrix.Inverse();
		LoopCount = std::abs(LoopCount);

		// 여러 바퀴도 반복 횟수의 제곱 분할로 합성한다.
		while (LoopCount >= 1.0)
		{
			if (std::fmod(LoopCount, 2.0) >= 1.0)
				LoopCorrection *= LoopMatrix;
			LoopCount = std::floor(LoopCount / 2.0);
			if (LoopCount >= 1.0)
				LoopMatrix = LoopMatrix * LoopMatrix;
		}
	}

	// 행 벡터 규약: Delta * Previous = Current (반복 경계 보정 포함).
	const FMatrix DeltaMatrix = CurrentMatrix * LoopCorrection * PreviousMatrix.Inverse();
	FTransform DeltaTransform = FTransform::Identity;
	FQuat DeltaRotation;
	if (!DeltaMatrix.Decompose(DeltaTransform.Location, DeltaRotation, DeltaTransform.Scale))
		return FTransform::Identity;
	DeltaTransform.Rotation = DeltaRotation.ToFRotator();
	return DeltaTransform;
}
