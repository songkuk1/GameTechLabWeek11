#include "EnginePCH.h"
#include "WidgetDrawElements.h"

void FWidgetWindowElementList::MakeBox(const FWidgetRect& Rect, UTexture2D* Texture, const FLinearColor& Color)
{
	FWidgetBoxElement& Element = BoxElements.AddDefaulted_GetRef();
	Element.Type = EWidgetDrawElementType::Box;
	Element.Rect = Rect;
	Element.Texture = Texture;
	Element.Color = Color;

	FWidgetElementInfo& ElementInfo = Batched.AddDefaulted_GetRef();
	ElementInfo.Type = EWidgetDrawElementType::Box;
	ElementInfo.Index = BoxElements.Num() - 1;
}

void FWidgetWindowElementList::MakeText(const FWidgetRect& Rect, const FString& Text, UFont* Atlas, const FLinearColor& Color)
{
	FWidgetTextElement& Element = TextElements.AddDefaulted_GetRef();
	Element.Type = EWidgetDrawElementType::Text;
	Element.Rect = Rect;
	Element.Text = Text;
	Element.Atlas = Atlas;
	Element.Color = Color;

	FWidgetElementInfo& ElementInfo = Batched.AddDefaulted_GetRef();
	ElementInfo.Type = EWidgetDrawElementType::Text;
	ElementInfo.Index = TextElements.Num() - 1;
}
