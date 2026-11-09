#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"
#include "SequencePlayerNode.h"
#include <cmath>

struct BlendSample
{
	AnimNode* Input = nullptr;
	float SampleValue;
};

class BlendSpace1DNode : public AnimNode
{
public:
	TArray<BlendSample> SampleArray;
	float Speed = 0.0f;
	float AnimationPhase = 0.0f;
	float Alpha = 0.0f;
	int LowIndex = -1;
	int HighIndex = -1;
	int LeaderIndex = -1;

	void AddSample(AnimNode* SampleInput, float SampleVal)
	{
		BlendSample Sample = { SampleInput, SampleVal };
		SampleArray.Add(Sample);
	}

	void Update(AnimationContext& AnimContext) override
	{
		LowIndex = HighIndex = -1;
		Alpha = 0.0f;
		// 1. Speed로 LowIndex, HighIndex, Alpha 계산
		Speed = AnimContext.Speed;
		const int32 SampleCount = SampleArray.Num();
		if (SampleCount == 0 || !std::isfinite(Speed))
		{
			LeaderIndex = -1;
			return;
		}

		// 샘플은 SampleValue 오름차순으로 등록한다. 범위 밖은 끝 샘플만 사용한다.
		if (SampleCount == 1 || Speed <= SampleArray[0].SampleValue)
			LowIndex = HighIndex = 0;
		else if (Speed >= SampleArray[SampleCount - 1].SampleValue)
			LowIndex = HighIndex = SampleCount - 1;
		else
		{
			for (int i = 0; i < SampleCount - 1; ++i)
			{
				const float LowValue = SampleArray[i].SampleValue;
				const float HighValue = SampleArray[i + 1].SampleValue;
				if (LowValue <= Speed && Speed < HighValue && HighValue > LowValue)
				{
					LowIndex = i;
					HighIndex = i + 1;
					Alpha = (Speed - LowValue) / (HighValue - LowValue);
					break;
				}
			}
		}

		if (!SampleArray.IsValidIndex(LowIndex) || !SampleArray.IsValidIndex(HighIndex)
			|| !SampleArray[LowIndex].Input || !SampleArray[HighIndex].Input)
		{
			LowIndex = HighIndex = LeaderIndex = -1;
			return;
		}

		// 가중치가 큰 샘플을 Leader로 선택 (동률이면 기존 Leader 유지)
		const int PreviousLeaderIndex = LeaderIndex;
		LeaderIndex = Alpha > 0.5f ? HighIndex : LowIndex;
		if (Alpha == 0.5f && (PreviousLeaderIndex == LowIndex || PreviousLeaderIndex == HighIndex)) LeaderIndex = PreviousLeaderIndex;
		const int FollowerIndex = LeaderIndex == LowIndex ? HighIndex : LowIndex;

		SequencePlayerNode* Leader = static_cast<SequencePlayerNode*>(SampleArray[LeaderIndex].Input);
		SequencePlayerNode* Follower = static_cast<SequencePlayerNode*>(SampleArray[FollowerIndex].Input);

		if (!Leader->GetSequence() || !Follower->GetSequence()) return;

		const float LeaderLength = Leader->GetSequence()->GetPlayLength();
		const float FollowerLength = Follower->GetSequence()->GetPlayLength();

		if (!(LeaderLength > 0.0f) || !(FollowerLength > 0.0f)) return;

		// Leader가 바뀌면 이전 프레임의 진행률 전달
		if (PreviousLeaderIndex >= 0 && PreviousLeaderIndex != LeaderIndex) Leader->SetCurrentPlayTime(AnimationPhase * LeaderLength);

		// Leader만 Update
		Leader->Update(AnimContext);

		// Leader 진행률로 Follower의 재생 시간 설정
		AnimationPhase = std::fmod(Leader->GetCurrentPlayTime(), LeaderLength) / LeaderLength;
		if (Follower != Leader) Follower->SetCurrentPlayTime(AnimationPhase * FollowerLength);
	}

	void EvaluatePose(PoseContext& Context) override
	{
		Context.LocalPoseTrans = Context.ReferencePose;
		if (!SampleArray.IsValidIndex(LowIndex) || !SampleArray.IsValidIndex(HighIndex)
			|| !SampleArray[LowIndex].Input || !SampleArray[HighIndex].Input) return;
		if (LowIndex == HighIndex || Alpha <= 0.0f)
		{
			SampleArray[LowIndex].Input->EvaluatePose(Context);
			return;
		}
		if (Alpha >= 1.0f)
		{
			SampleArray[HighIndex].Input->EvaluatePose(Context);
			return;
		}

		PoseContext ContextA = Context;
		PoseContext ContextB = Context;

		SampleArray[LowIndex].Input->EvaluatePose(ContextA);
		SampleArray[HighIndex].Input->EvaluatePose(ContextB);

		// 본마다 위치 / 스케일은 Lerp, 회전은 Quaternion Slerp
		BlendTwoPosesTogether(ContextA.LocalPoseTrans, ContextB.LocalPoseTrans, Alpha, Context.LocalPoseTrans);
	}

	void BlendTwoPosesTogether(const TArray<PoseTransform>& APoseTrans, const TArray<PoseTransform>& BPoseTrans, float Alphaval, TArray<PoseTransform>& OutLocalPoseTrans)
	{
		for (int i = 0;i < OutLocalPoseTrans.Num();i++)
		{
			OutLocalPoseTrans[i].Translation = FVector(
				std::lerp(APoseTrans[i].Translation.X, BPoseTrans[i].Translation.X, Alphaval),
				std::lerp(APoseTrans[i].Translation.Y, BPoseTrans[i].Translation.Y, Alphaval),
				std::lerp(APoseTrans[i].Translation.Z, BPoseTrans[i].Translation.Z, Alphaval)
			);
			OutLocalPoseTrans[i].Rotation = FQuat::Slerp(APoseTrans[i].Rotation, BPoseTrans[i].Rotation, Alphaval);
			OutLocalPoseTrans[i].Scale = FVector(
				std::lerp(APoseTrans[i].Scale.X, BPoseTrans[i].Scale.X, Alphaval),
				std::lerp(APoseTrans[i].Scale.Y, BPoseTrans[i].Scale.Y, Alphaval),
				std::lerp(APoseTrans[i].Scale.Z, BPoseTrans[i].Scale.Z, Alphaval)
			);
		}
	}

};
