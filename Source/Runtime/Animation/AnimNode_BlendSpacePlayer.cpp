#include "EnginePCH.h"
#include "Animation/AnimNode_BlendSpacePlayer.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include <algorithm>
#include <cmath>

FAnimNode_BlendSpacePlayer::FAnimNode_BlendSpacePlayer()
{
	PlayerA.SetAllowRootMotion(false);
	PlayerB.SetAllowRootMotion(false);
	BlendNode.SetSourceNodes(&PlayerA, &PlayerB);
}

void FAnimNode_BlendSpacePlayer::SetBlendSpace(UBlendSpace1D* InBlendSpace)
{
	BlendSpace = InBlendSpace;
	PreviousSampleA = -1;
	PreviousSampleB = -1;
	NormalizedTime = 0.0f;
	PlayerA.SetSequence(nullptr);
	PlayerB.SetSequence(nullptr);
}

void FAnimNode_BlendSpacePlayer::SetBlendInput(float InInput)
{
	BlendInput = std::isfinite(InInput) ? InInput : 0.0f;
}

void FAnimNode_BlendSpacePlayer::SetBlendParameter(const FName& InParameterName)
{
	BlendParameterName = InParameterName;
}

void FAnimNode_BlendSpacePlayer::SetPlayRate(float InPlayRate)
{
	PlayRate = std::isfinite(InPlayRate) ? InPlayRate : 1.0f;
}

void FAnimNode_BlendSpacePlayer::SetLooping(bool bInLoop)
{
	bLoop = bInLoop;
}

void FAnimNode_BlendSpacePlayer::SetPlaybackPosition(float InTime)
{
	const float ReferenceLength = GetPlayLength();

	if (ReferenceLength <= 0.0f || !std::isfinite(InTime))
	{
		return;
	}

	NormalizedTime = std::clamp(InTime / ReferenceLength, 0.0f, 1.0f);

	const float LengthA = PlayerA.GetPlayLength();
	const float LengthB = PlayerB.GetPlayLength();

	if (LengthA > 0.0f)
	{
		PlayerA.SetPlaybackPosition(NormalizedTime * LengthA);
	}

	if (LengthB > 0.0f)
	{
		PlayerB.SetPlaybackPosition(NormalizedTime * LengthB);
	}
}

float FAnimNode_BlendSpacePlayer::GetCurrentTime() const
{
    return NormalizedTime * GetPlayLength();
}

float FAnimNode_BlendSpacePlayer::GetPlayLength() const
{
	if (!BlendSpace)
	{
		return 0.0f;
	}
	float MaxLength = 0.0f;
	for (const FBlendSample1D& Sample : BlendSpace->GetSamples())
	{
		if (!Sample.Animation || !Sample.Animation->GetDataModel())
		{
			continue;
		}
		MaxLength = std::max(MaxLength, Sample.Animation->GetDataModel()->PlayLength);
	}
	return MaxLength;
}

float FAnimNode_BlendSpacePlayer::GetFrameRate() const
{
	if (!BlendSpace)
	{
		return 0.0f;
	}
	const UAnimSequence* ReferenceAnimation = nullptr;
	float MaxLength = 0.0f;

	for (const FBlendSample1D& Sample : BlendSpace->GetSamples())
	{
		if (!Sample.Animation || !Sample.Animation->GetDataModel())
		{
			continue;
		}
		const float Length = Sample.Animation->GetDataModel()->PlayLength;
		if (Length > MaxLength)
		{
			MaxLength = Length;
			ReferenceAnimation = Sample.Animation;
		}
	}
	return ReferenceAnimation ? ReferenceAnimation->GetDataModel()->FrameRate.AsDecimal() : 0.0f;
}

void FAnimNode_BlendSpacePlayer::Update(const FAnimationUpdateContext& Context)
{
	if (!BlendParameterName.IsNone())
	{
		if (Context.AnimInstance && Context.AnimInstance->HasFloatParameter(BlendParameterName))
		{
			SetBlendInput(Context.AnimInstance->GetFloatParameter(BlendParameterName));
		}
		else
		{
			SetBlendInput(0.0f);
		}
	}

	if (!BlendSpace)
	{
		return;
	}

	int32 SampleIndexA = -1;
	int32 SampleIndexB = -1;
	float Alpha = 0.0f;

	if (!BlendSpace->GetBlendSamples(BlendInput, SampleIndexA, SampleIndexB, Alpha))
	{
		return;
	}
	const TArray<FBlendSample1D>& Samples = BlendSpace->GetSamples();
	const FBlendSample1D& SampleA = Samples[SampleIndexA];
	const FBlendSample1D& SampleB = Samples[SampleIndexB];
	if (!SampleA.Animation || !SampleB.Animation)
	{
		return;
	}
	const bool bSampleChanged = PreviousSampleA != SampleIndexA || PreviousSampleB != SampleIndexB;
	if (bSampleChanged)
	{
		if (PreviousSampleB == SampleIndexA && PlayerB.Sequence == SampleA.Animation)
		{
			const float OldLengthB = PlayerB.GetPlayLength();
			if (OldLengthB > 0.0f)
			{
				NormalizedTime = std::clamp(PlayerB.GetCurrentTime() / OldLengthB, 0.0f, 1.0f);
			}
		}
		else if (PreviousSampleA == SampleIndexB && PlayerA.Sequence == SampleB.Animation)
		{
			const float OldLengthA = PlayerA.GetPlayLength();
			if (OldLengthA > 0.0f)
			{
				NormalizedTime = std::clamp(PlayerA.GetCurrentTime() / OldLengthA, 0.0f, 1.0f);
			}
		}

		PlayerA.SetSequence(SampleA.Animation);
		PlayerB.SetSequence(SampleB.Animation);

		PreviousSampleA = SampleIndexA;
		PreviousSampleB = SampleIndexB;
	}

	const float LengthA = PlayerA.GetPlayLength();
	const float LengthB = PlayerB.GetPlayLength();

	if (LengthA <= 0.0f || LengthB <= 0.0f)
	{
		return;
	}

	if (bSampleChanged)
	{
		PlayerA.SetPlaybackPosition(NormalizedTime * LengthA);
		PlayerB.SetPlaybackPosition(NormalizedTime * LengthB);
	}

	const bool bBlendingFromIdle = (SampleIndexA != SampleIndexB) && (SampleA.SampleValue <= 0.001f);
	if (bBlendingFromIdle)
	{
		PlayerA.SetPlayRate(PlayRate * SampleA.RateScale);
		PlayerB.SetPlayRate(PlayRate * SampleB.RateScale);
	}
	else
	{
		const float BlendedLength = std::max(LengthA + (LengthB - LengthA) * Alpha, 1.0e-4f);
		PlayerA.SetPlayRate(PlayRate * (LengthA / BlendedLength) * SampleA.RateScale);
		PlayerB.SetPlayRate(PlayRate * (LengthB / BlendedLength) * SampleB.RateScale);
	}

	PlayerA.SetLooping(bLoop);
	PlayerB.SetLooping(bLoop);

	BlendNode.SetAlpha(Alpha);

	BlendNode.Update(Context);

	if (bBlendingFromIdle)
	{
		NormalizedTime = std::clamp(PlayerB.GetCurrentTime() / LengthB, 0.0f, 1.0f);
	}
	else
	{
		NormalizedTime = std::clamp(PlayerA.GetCurrentTime() / LengthA, 0.0f, 1.0f);
	}
}

void FAnimNode_BlendSpacePlayer::Evaluate(FPoseContext& Output)
{
	BlendNode.Evaluate(Output);
}

bool FAnimNode_BlendSpacePlayer::EvaluateCurve(const FName& CurveName, const FName& LayerName, float& OutValue) const
{
	return BlendNode.EvaluateCurve(CurveName, LayerName, OutValue);
}