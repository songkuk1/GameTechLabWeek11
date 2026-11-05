#pragma once

#include "Containers/Array.h"
#include "FrameTime.h"
#include "MovieSceneFloatValue.h"

class FStructuredArchiveSlot;

struct FMovieSceneFloatChannel
{
public:
	void AddKey(int32 InTime, const FMovieSceneFloatValue& InValue);

	void UpdateOrAddKey(int32 InTime, const FMovieSceneFloatValue& InValue);

	void RemoveKey(int32 InTime);

	void MoveKey(int32 InTime, int32 InNewTime);

	float DoInterpolation(int32 TimeA, FMovieSceneFloatValue* ValueA, int32 TimeB, FMovieSceneFloatValue* ValueB, const FFrameTime& InTime);

	bool Evaluate(const FFrameTime& InTime, float& OutValue);

	void SetDefault(float InDefaultValue);

	const TArray<int32>& GetTimes() const;

	const TArray<FMovieSceneFloatValue>& GetValues() const;

	bool HasDefaultValue() const;

	float GetDefaultValue() const;

private:
	int32 FindKeyIndex(int32 InTime);

private:
	friend void operator<<(FStructuredArchiveSlot Slot, FMovieSceneFloatChannel& V);

	TArray<int32> Times;
	TArray<FMovieSceneFloatValue> Values;

	bool bHasDefaultValue = false;
	float DefaultValue = 0.f;
};
