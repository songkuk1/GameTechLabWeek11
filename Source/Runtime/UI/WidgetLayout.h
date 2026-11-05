#pragma once

#include "Core/Types.h"
#include "Math/Vector2.h"

using FWidgetID = int32;

constexpr FWidgetID RootCanvasID = 0;

struct FWidgetLayout
{
	FVector2 AnchorMin, AnchorMax; // 부모 기준 앵커 (0 ~ 1)
	FVector2 Pivot; // 위젯 내부 기준점 (0 ~ 1)
	FVector2 AnchoredPosition; // 앵커 기준 위치
	FVector2 SizeDelta; // 앵커 간 크기에 다할 값

	FWidgetLayout();
	FWidgetLayout(const FVector2& InAnchorMin, const FVector2& InAnchorMax, const FVector2& InPivot, const FVector2& InAnchoredPosition, const FVector2& InSizeDelta)
		: AnchorMin(InAnchorMin)
		, AnchorMax(InAnchorMax)
		, Pivot(InPivot)
		, AnchoredPosition(InAnchoredPosition)
		, SizeDelta(InSizeDelta)
	{
	}

	FVector2 GetOffsetMin() const;
	FVector2 GetOffsetMax() const;
};

struct FWidgetRect
{
	FVector2 Min;
	FVector2 Max;

	FWidgetRect() = default;

	FWidgetRect(float InMinX, float InMinY, float InMaxX, float InMaxY);

	FWidgetRect(const FVector2& InMin, const FVector2& InMax);

	float Width() const;
	float Height() const;
	FVector2 Size() const;
};
