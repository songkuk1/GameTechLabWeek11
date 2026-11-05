#pragma once

#include "Widget.h"
#include "Core/EngineString.h"
#include "Math/Color.h"

class UFont;

class STextWidget : public SWidget
{
public:
	STextWidget(FWidgetID ID);

	void SetText(const FString& InText);

	void SetFont(UFont* InFont);

	void SetColor(const FLinearColor& InColor);

	virtual void OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList) override;

private:
	FString Text;
	UFont* Font;
	FLinearColor Color;
};
