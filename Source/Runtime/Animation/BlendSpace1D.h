#pragma once

#include "Animation/AnimationAsset.h"
#include "Containers/Array.h"

class UAnimSequence;

struct FBlendSample1D
{
	float SampleValue = 0.0f;
	UAnimSequence* Animation = nullptr;
};

class UBlendSpace1D : public UAnimationAsset
{
	DECLARE_CLASS(UBlendSpace1D, UAnimationAsset)
public:
	bool AddSample(float SampleValue, UAnimSequence* Animation);
	const TArray<FBlendSample1D>& GetSamples() const { return Samples; }
	bool GetBlendSamples(float Input, int32& OutIndexA, int32 OutIndexB, float& OutAlpha) const;
	void ClearSamples() { Samples.Empty(); }
private:
	TArray<FBlendSample1D> Samples;
};