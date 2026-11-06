#pragma once

#include "UObject/Object.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

// 애니메이션 키 데이터
struct FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	TArray<FVector> ScaleKeys;
};

// 본별 애니메이션 데이터
struct FBoneAnimationTrack
{
	FName Name;
	int32 NodeIndex = -1;
	FRawAnimSequenceTrack InternalTrack;
};

// 프레임 레이트
struct FFrameRate
{
	int32 Numerator = 30;
	int32 Denominator = 1;
	bool IsValid() const { return Numerator > 0 && Denominator > 0; }
	float AsDecimal() const { return IsValid() ? static_cast<float>(Numerator) / Denominator : 0.0f; }
};

// 애니메이션 커브 데이터
struct FAnimationCurveData
{ 

};

//애니메이션 데이터 접근 인터페이스
class IAnimationDataModel
{
public:
	virtual ~IAnimationDataModel() = default;
	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const = 0;
};

// 실제 애니메이션 데이터 모델
class UAnimDataModel : public UObject, public IAnimationDataModel
{
	DECLARE_CLASS(UAnimDataModel, UObject)
public:
	TArray<FBoneAnimationTrack> BoneAnimationTracks;
	float PlayLength = 0.0f;
	FFrameRate FrameRate;
	int32 NumberOfFrames = 0;
	int32 NumberOfKeys = 0;
	FAnimationCurveData CurveData;
	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const override { return BoneAnimationTracks; }
};