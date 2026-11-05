#pragma once

#include "Core/Types.h"

struct FFrameTime
{
	int32 FrameNumber;
	float SubFrame;

	float AsDecimal();

	static FFrameTime FromDecimal(float InDecimal);
};
