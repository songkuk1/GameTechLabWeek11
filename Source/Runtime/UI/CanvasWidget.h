#pragma once

#include "Widget.h"
#include "Containers/Array.h"

class SCanvasWidget : public SWidget
{
public:
	SCanvasWidget(FWidgetID ID);

	void AddChild(SWidget* Widget);

	TArray<SWidget*>& GetChildren();

	virtual void OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList) override;

private:
	TArray<SWidget*> Widgets;
};
