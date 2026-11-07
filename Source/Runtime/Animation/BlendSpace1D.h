#pragma once

#include "Animation/AnimationAsset.h"
#include "Containers/Array.h"

class UAnimSequence;
class USkeleton;
class USkeletalMesh;

struct FBlendSample1D
{
	float SampleValue = 0.0f;
	UAnimSequence* Animation = nullptr;
	float RateScale = 1.0f;
};

struct FBlendSpaceAxis1D
{
	FName Name = FName("Speed");
	float MinValue = 0.0f;
	float MaxValue = 600.0f;
	int32 GridDivisions = 6;
};

class UBlendSpace1D : public UAnimationAsset
{
	DECLARE_CLASS(UBlendSpace1D, UAnimationAsset)
public:
	const FBlendSpaceAxis1D& GetAxis() const { return Axis; }
	bool SetAxis(const FBlendSpaceAxis1D& InAxis);
	bool AddSample(float SampleValue, UAnimSequence* Animation, float RateScale = 1.0f);
	bool RemoveSample(int32 SampleIndex);
	bool SetSampleValue(int32 SampleIndex, float SampleValue);
	bool ReplaceSampleAnimation(int32 SampleIndex, UAnimSequence* Animation);
	bool SetSampleRateScale(int32 SampleIndex, float RateScale);
	const TArray<FBlendSample1D>& GetSamples() const { return Samples; }
	bool GetBlendSamples(float Input, int32& OutIndexA, int32& OutIndexB, float& OutAlpha) const;
	void ClearSamples() { Samples.Empty(); }
	bool SetSkeleton(USkeleton* InSkeleton);
	USkeleton* GetSkeleton() const { return Skeleton; }
	bool SetPreviewMesh(USkeletalMesh* InPreviewMesh);
	USkeletalMesh* GetPreviewMesh() const { return PreviewMesh; }

	bool IsBlendSpaceDataValid() const;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
private:
	FBlendSpaceAxis1D Axis;
	TArray<FBlendSample1D> Samples;
	USkeleton* Skeleton = nullptr;
	USkeletalMesh* PreviewMesh = nullptr;
	bool bSerializationValid = true;
};