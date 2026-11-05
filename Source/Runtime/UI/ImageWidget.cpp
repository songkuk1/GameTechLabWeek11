#include "EnginePCH.h"
#include "ImageWidget.h"
#include "WidgetDrawElements.h"

SImageWidget::SImageWidget(FWidgetID ID) : SWidget(ID)
{
}

void SImageWidget::SetTexture(UTexture2D* InTexture)
{
	Texture = InTexture;
}

void SImageWidget::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
}

void SImageWidget::OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList)
{
	OutDrawElementList.MakeBox(CalculateWorldRect(ParentRect), Texture, Color);
}
