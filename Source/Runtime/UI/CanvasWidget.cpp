#include "EnginePCH.h"
#include "CanvasWidget.h"

SCanvasWidget::SCanvasWidget(FWidgetID ID)
	: SWidget(ID)
{
}

void SCanvasWidget::AddChild(SWidget* Widget)
{
	Widgets.Add(Widget);
}

TArray<SWidget*>& SCanvasWidget::GetChildren()
{
	return Widgets;
}

void SCanvasWidget::OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList)
{
	FWidgetRect WorldRect = CalculateWorldRect(ParentRect);

	for (SWidget* Widget : Widgets)
	{
		Widget->OnPaint(WorldRect, OutDrawElementList);
	}
}
