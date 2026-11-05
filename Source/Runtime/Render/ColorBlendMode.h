#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

// HLSL 상수와 값을 공유하는 범용 색상 합성 방식
enum class EColorBlendMode : uint32
{
    Alpha = 0,
    Multiply = 1
};

namespace ColorBlending
{
    // 색상 합성 방식의 저장 및 표시 이름 변환
    inline const char* ToString(EColorBlendMode Mode)
    {
        return Mode == EColorBlendMode::Multiply ? "Multiply" : "Alpha";
    }

    // 저장 이름의 합성 방식 복원 및 기본 Alpha 처리
    inline EColorBlendMode FromString(const FString& Name)
    {
        return Name == "Multiply" ? EColorBlendMode::Multiply : EColorBlendMode::Alpha;
    }
}