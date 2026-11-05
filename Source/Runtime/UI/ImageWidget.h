#pragma once

#include "Widget.h"
#include "Math/Color.h"

class UTexture2D;

class SImageWidget : public SWidget
{
public:
	SImageWidget(FWidgetID ID);

	void SetTexture(UTexture2D* InTexture);

	void SetColor(const FLinearColor& InColor);

	virtual void OnPaint(const FWidgetRect& ParentRect, FWidgetWindowElementList& OutDrawElementList) override;

private:
	UTexture2D* Texture;
	FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 1.f);
};
