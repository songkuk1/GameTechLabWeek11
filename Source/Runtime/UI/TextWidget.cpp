#include "EnginePCH.h"
#include "TextWidget.h"
#include "WidgetDrawElements.h"

STextWidget::STextWidget(FWidgetID ID) : SWidget(ID)
{
}

void STextWidget::SetText(const FString& InText)
{
	Text = InText;
}

void STextWidget::SetFont(UFont* InFont)
{
	Font = InFont;
}

void STextWidget::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
}

void STextWidget::OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList)
{
	OutDrawElementList.MakeText(CalculateWorldRect(ParentRect), Text, Font, Color);
}
