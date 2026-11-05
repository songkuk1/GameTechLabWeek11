#pragma once

#include "WidgetLayout.h"

class FWidgetWindowElementList;

class SWidget
{
public:
	SWidget(FWidgetID ID);

	virtual ~SWidget() = default;

	virtual void OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList) = 0;

	void SetLayout(const FWidgetLayout& InLayout);

	inline const FWidgetLayout& GetLayout()
	{
		return Layout;
	}

	FWidgetID GetID() const;

protected:
	FWidgetRect CalculateWorldRect(const FWidgetRect& ParentRect);

protected:
	FWidgetID ID;
	FWidgetLayout Layout;
};
