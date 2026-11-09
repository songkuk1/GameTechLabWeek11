#pragma once

#include "EnginePCH.h"
#include "AnimNode.h"

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

	void AddSample(AnimNode* SampleInput, float SampleVal)
	{
		BlendSample Sample = { SampleInput, SampleVal };
		SampleArray.Add(Sample);
	}

	void SetSpeed(float InSpeed) { Speed = InSpeed; }

	void Update(AnimationContext& AnimContext) override
	{
		for (int i = 0;i < SampleArray.Num();i++)
		{
			if (SampleArray[i].Input)
				SampleArray[i].Input->Update(AnimContext);   // 전체 입력에 대해서 Update 진행
		}
	}

	void EvaluatePose(PoseContext& Context) override
	{
		Context.LocalPoseTrans = Context.ReferencePose;
		const int32 SampleCount = SampleArray.Num();

		if (SampleCount == 0 || !std::isfinite(Speed))
			return;

		if (SampleCount == 1 || Speed <= SampleArray[0].SampleValue)
		{
			if (SampleArray[0].Input) SampleArray[0].Input->EvaluatePose(Context);
			return;
		}
		if (Speed >= SampleArray[SampleCount - 1].SampleValue)
		{
			if (SampleArray[SampleCount - 1].Input) SampleArray[SampleCount - 1].Input->EvaluatePose(Context);
			return;
		}

		float lowSample = 0.0f;
		int32 lowindex = -1;
		float highSample = 0.0f;
		int32 highindex = -1;
		for (int i = 0;i < SampleArray.Num()-1; i++)
		{
			if (SampleArray[i].SampleValue <= Speed && Speed < SampleArray[i + 1].SampleValue)
			{
				lowSample = SampleArray[i].SampleValue;
				lowindex = i;
				highSample = SampleArray[i + 1].SampleValue;
				highindex = i+1;
				break;
			}
		}
		if (lowindex == -1 || highindex == -1|| !SampleArray[lowindex].Input || !SampleArray[highindex].Input|| !(highSample > lowSample)) return;

		float Alpha = (Speed - lowSample) / (highSample - lowSample);
		PoseContext ContextA = Context;
		PoseContext ContextB = Context;

		SampleArray[lowindex].Input->EvaluatePose(ContextA);
		SampleArray[highindex].Input->EvaluatePose(ContextB);

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
