#pragma once

#include "Core/Types.h"

enum class ECurveInterpMode : int8
{
	Constant, // A값을 유지하다가 B로 변경
	Linear // 일정한 비율로 보간
};

struct FMovieSceneFloatValue
{
	ECurveInterpMode InterpMode = ECurveInterpMode::Constant;
	float Value = 0.f;
};
