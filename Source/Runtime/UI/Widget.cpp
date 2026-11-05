#include "EnginePCH.h"
#include "Widget.h"

SWidget::SWidget(FWidgetID ID)
	: ID(ID)
{
}

void SWidget::SetLayout(const FWidgetLayout& InLayout)
{
	Layout = InLayout;
}

FWidgetID SWidget::GetID() const
{
	return ID;
}

FWidgetRect SWidget::CalculateWorldRect(const FWidgetRect& ParentRect)
{
	return FWidgetRect(
		ParentRect.Min + ParentRect.Size() * Layout.AnchorMin + Layout.GetOffsetMin(),
		ParentRect.Min + ParentRect.Size() * Layout.AnchorMax + Layout.GetOffsetMax()
	);
}
