#pragma once

#include "WidgetLayout.h"
#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/Color.h"

class UTexture2D;
class UFont;

enum class EWidgetDrawElementType
{
	Box,
	Text
};

struct FWidgetDrawElement
{
	EWidgetDrawElementType Type;
	FWidgetRect Rect;
};

struct FWidgetBoxElement : public FWidgetDrawElement
{
	UTexture2D* Texture;
	FLinearColor Color;
};

struct FWidgetTextElement : public FWidgetDrawElement
{
	FString Text;
	UFont* Atlas;
	FLinearColor Color;
};

struct FWidgetElementInfo
{
	EWidgetDrawElementType Type;
	int32 Index;
};

class FWidgetWindowElementList
{
public:
	void MakeBox(const FWidgetRect& Rect, UTexture2D* Texture, const FLinearColor& Color);

	void MakeText(const FWidgetRect& Rect, const FString& Text, UFont* Atlas, const FLinearColor& Color);

private:
	friend class UGameViewportClient;

	TArray<FWidgetBoxElement> BoxElements;
	TArray<FWidgetTextElement> TextElements;

	TArray<FWidgetElementInfo> Batched;
};
