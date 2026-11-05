#pragma once

#include "Core/Types.h"

// 그림자 필터 방식. 샘플러(Point/Comparison)와 반경 방식(고정/가변)을 고른다.
// UberLit.hlsl의 SHADOW_FILTER_* 값과 순서가 같아야 한다.
enum class EShadowFilterMode : uint32
{
	Hard = 0,		// Point 샘플러 보간 없음
	HardwarePCF,	// Comparison LINEAR 
	PCF,			// Comparison + 고정 반경
	PCSS,			// 동적 반경
	Count
};

inline constexpr const char* ShadowFilterModeNames[] = { "Hard", "HardwarePCF", "PCF", "PCSS" };

// Renderer가 받는 그림자 필터 설정. Editor 타입에 의존하지 않도록 Runtime에 둔다.
// Poisson, Gaussian, Rotation은 다중 탭 방식(PCF, PCSS)에만 적용된다.
struct FShadowFilterSettings
{
	EShadowFilterMode Mode = EShadowFilterMode::PCSS;
	bool bPoisson = true;		// 꺼지면 5x5 격자
	bool bGaussian = false;		// 탭 가중치 (필터 단계에만)
	bool bRotate = true;		// Poisson을 픽셀별 회전 (Poisson일 때만)
	float PCFRadiusTexels = 1.5f;	// PCF 고정 반경
	float MaxFilterTexels = 32.0f;	// PCSS 필터 반경 상한
	float SearchScale = 1.5f;		// PCSS 탐색 반경 상한 = 필터 상한 x 배율보다 커야 반그림자 바깥 끝이 끊기지 않는다
};

// UberLit.hlsl의 ShadowFilterConstants(b6)와 같은 레이아웃
struct FShadowFilterConstants
{
	uint32 Mode = 0;
	uint32 bPoisson = 0;
	uint32 bGaussian = 0;
	uint32 bRotate = 0;
	float PCFRadiusTexels = 0.0f;
	float MaxFilterTexels = 0.0f;
	float SearchScale = 0.0f;
	float Padding = 0.0f;
};
static_assert(sizeof(FShadowFilterConstants) == 32);
