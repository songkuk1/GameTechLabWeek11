#include "EnginePCH.h"
#include "WidgetLayout.h"

FWidgetLayout::FWidgetLayout()
	: AnchorMin(0.f, 0.f)
	, AnchorMax(0.f, 0.f)
	, Pivot(0.5f, 0.5f)
	, AnchoredPosition(0.f, 0.f)
	, SizeDelta(0.f, 0.f)
{
}

FVector2 FWidgetLayout::GetOffsetMin() const
{
	return AnchoredPosition - SizeDelta * Pivot;
}

FVector2 FWidgetLayout::GetOffsetMax() const
{
	return AnchoredPosition + SizeDelta * FVector2(1.f - Pivot.X, 1.f - Pivot.Y);
}

FWidgetRect::FWidgetRect(float InMinX, float InMinY, float InMaxX, float InMaxY)
	: Min(InMinX, InMinY)
	, Max(InMaxX, InMaxY)
{
}

FWidgetRect::FWidgetRect(const FVector2& InMin, const FVector2& InMax)
	: Min(InMin), Max(InMax)
{
}

float FWidgetRect::Width() const
{
	return Max.X - Min.X;
}

float FWidgetRect::Height() const
{
	return Max.Y - Min.Y;
}

FVector2 FWidgetRect::Size() const
{
	return FVector2(Width(), Height());
}
