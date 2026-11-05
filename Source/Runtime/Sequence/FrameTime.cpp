#include "EnginePCH.h"
#include "FrameTime.h"

float FFrameTime::AsDecimal()
{
	return (float)FrameNumber + SubFrame;
}

FFrameTime FFrameTime::FromDecimal(float InDecimal)
{
	int32 FrameNumber = (int32)InDecimal;
	float SubFrame = InDecimal - (float)FrameNumber;

	return FFrameTime{ FrameNumber, SubFrame };
}
