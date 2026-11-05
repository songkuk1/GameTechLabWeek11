#include "EnginePCH.h"
#include "MovieSceneFloatChannel.h"
#include "Math/EngineMath.h"
#include <stdexcept>
#include <utility>

void FMovieSceneFloatChannel::AddKey(int32 InTime, const FMovieSceneFloatValue& InValue)
{
	Times.Add(InTime);
	Values.Emplace(InValue);

	for (int32 i = Times.Num() - 1; i > 0; i--)
	{
		if (Times[i] < Times[i - 1])
		{
			std::swap(Times[i], Times[i - 1]);
			std::swap(Values[i], Values[i - 1]);
		}
		else
		{
			break;
		}
	}
}

void FMovieSceneFloatChannel::UpdateOrAddKey(int32 InTime, const FMovieSceneFloatValue& InValue)
{
	int32 Target = FindKeyIndex(InTime);
	if (Target != -1)
	{
		Values[Target] = InValue;
		return;
	}

	AddKey(InTime, InValue);
}

void FMovieSceneFloatChannel::RemoveKey(int32 InTime)
{
	int32 Target = FindKeyIndex(InTime);
	if (Target == -1)
	{
		return;
	}

	for (int32 i = Target; i < Times.Num() - 1; i++)
	{
		std::swap(Times[i], Times[i + 1]);
		std::swap(Values[i], Values[i + 1]);
	}

	Times.Pop();
	Values.Pop();
}

void FMovieSceneFloatChannel::MoveKey(int32 InTime, int32 InNewTime)
{
	if (InTime == InNewTime)
	{
		return;
	}

	int32 Target = FindKeyIndex(InTime);
	if (Target == -1)
	{
		return;
	}

	Times[Target] = InNewTime;
	if (InTime < InNewTime)
	{
		for (int32 i = Target; i > 0; i--)
		{
			if (Times[i] > Times[i - 1])
			{
				break;
			}

			std::swap(Times[i], Times[i - 1]);
			std::swap(Values[i], Values[i - 1]);
		}
	}
	else if (InTime > InNewTime)
	{
		for (int32 i = Target; i < Times.Num() - 1; i++)
		{
			if (Times[i] < Times[i + 1])
			{
				break;
			}

			std::swap(Times[i], Times[i + 1]);
			std::swap(Values[i], Values[i + 1]);
		}
	}
}

float FMovieSceneFloatChannel::DoInterpolation(int32 TimeA, FMovieSceneFloatValue* ValueA, int32 TimeB, FMovieSceneFloatValue* ValueB, const FFrameTime& InTime)
{
	switch (ValueA->InterpMode)
	{
		case ECurveInterpMode::Constant: return ValueA->Value;
		case ECurveInterpMode::Linear: return FMath::Lerp(ValueA->Value, ValueB->Value, ((float)(InTime.FrameNumber - TimeA) + InTime.SubFrame) / (float)(TimeB - TimeA));
	}

	throw std::runtime_error("Unknown interporation type!");
}

bool FMovieSceneFloatChannel::Evaluate(const FFrameTime& InTime, float& OutValue)
{
	for (int32 i = 0; i < Times.Num(); i++)
	{
		if (InTime.FrameNumber < Times[i])
		{
			// NOTE: 다음 프레임 찾음!
			if (i == 0)
			{
				// NOTE: 첫 프레임이므로 바로 반환
				OutValue = Values[i].Value;
			}
			else
			{
				OutValue = DoInterpolation(Times[i - 1], &Values[i - 1], Times[i], &Values[i], InTime);
			}

			return true;
		}
	}

	if (Times.Num())
	{
		OutValue = Values.Last().Value;
	}
	else if (bHasDefaultValue)
	{
		OutValue = DefaultValue;
	}
	else
	{
		return false;
	}

	return true;
}

void FMovieSceneFloatChannel::SetDefault(float InDefaultValue)
{
	bHasDefaultValue = true;
	DefaultValue = InDefaultValue;
}

const TArray<int32>& FMovieSceneFloatChannel::GetTimes() const
{
	return Times;
}

const TArray<FMovieSceneFloatValue>& FMovieSceneFloatChannel::GetValues() const
{
	return Values;
}

bool FMovieSceneFloatChannel::HasDefaultValue() const
{
	return bHasDefaultValue;
}

float FMovieSceneFloatChannel::GetDefaultValue() const
{
	return DefaultValue;
}

int32 FMovieSceneFloatChannel::FindKeyIndex(int32 InTime)
{
	int32 Target = -1;
	for (int32 i = 0; i < Times.Num(); i++)
	{
		if (InTime < Times[i])
		{
			break;
		}

		if (InTime == Times[i])
		{
			Target = i;
			break;
		}
	}

	return Target;
}
