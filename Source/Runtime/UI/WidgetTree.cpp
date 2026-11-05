#include "EnginePCH.h"
#include "WidgetTree.h"

#include "CanvasWidget.h"
#include "TextWidget.h"
#include "ImageWidget.h"
#include "Asset/AssetManager.h"
#include "Core/ResourcePaths.h"

static SWidget* CreateWidgetsImpl(json InJson, SCanvasWidget* Canvas)
{
	FString TypeStr = InJson["Type"].get<std::string>();
	int32 ID = InJson["ID"].get<int32>();

	const json& LayoutJson = InJson["Layout"];
	FWidgetLayout Layout;
	Layout.AnchorMin = FVector2(LayoutJson["AnchorMin"][0].get<float>(), LayoutJson["AnchorMin"][1].get<float>());
	Layout.AnchorMax = FVector2(LayoutJson["AnchorMax"][0].get<float>(), LayoutJson["AnchorMax"][1].get<float>());
	Layout.Pivot = FVector2(LayoutJson["Pivot"][0].get<float>(), LayoutJson["Pivot"][1].get<float>());
	Layout.AnchoredPosition = FVector2(LayoutJson["AnchoredPosition"][0].get<float>(), LayoutJson["AnchoredPosition"][1].get<float>());
	Layout.SizeDelta = FVector2(LayoutJson["SizeDelta"][0].get<float>(), LayoutJson["SizeDelta"][1].get<float>());

	SWidget* Widget = nullptr;
	if (TypeStr == "Canvas")
	{
		SCanvasWidget* NewCanvas = new SCanvasWidget(ID);

		json ChildrenJson = InJson["Children"];
		for (json Element : ChildrenJson)
		{
			CreateWidgetsImpl(Element, NewCanvas);
		}

		Widget = NewCanvas;
	}
	else if (TypeStr == "Text")
	{
		FString InitString = InJson["Text"].get<std::string>();

		UFont* Font = UAssetManager::GetAssetByPath<UFont>(ResourcePaths::DefaultFont);
		if (InJson.contains("Font"))
		{
			FString FontURL = InJson["Font"].get<std::string>();
			Font = UAssetManager::GetAssetByPath<UFont>(FontURL);
		}

		FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 1.f);
		if (InJson.contains("Color"))
		{
			json ColorJson = InJson["Color"];
			Color = FLinearColor(
				ColorJson[0].get<float>(),
				ColorJson[1].get<float>(),
				ColorJson[2].get<float>(),
				ColorJson[3].get<float>()
			);
		}

		STextWidget* NewTextWidget = new STextWidget(ID);
		NewTextWidget->SetText(InitString);
		NewTextWidget->SetFont(Font);
		NewTextWidget->SetColor(Color);

		Widget = NewTextWidget;
	}
	else if (TypeStr == "Image")
	{
		UTexture2D* Texture = nullptr;
		if (InJson.contains("Texture"))
		{
			FString TextureURL = InJson["Texture"].get<std::string>();
			Texture = UAssetManager::GetAssetByPath<UTexture2D>(TextureURL);
		}

		FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 1.f);
		if (InJson.contains("Color"))
		{
			json ColorJson = InJson["Color"];
			Color = FLinearColor(
				ColorJson[0].get<float>(),
				ColorJson[1].get<float>(),
				ColorJson[2].get<float>(),
				ColorJson[3].get<float>()
			);
		}

		SImageWidget* NewImageWidget = new SImageWidget(ID);
		NewImageWidget->SetTexture(Texture);
		NewImageWidget->SetColor(Color);

		Widget = NewImageWidget;
	}
	else
	{
		throw std::runtime_error("Unknown widget type!");
	}

	Widget->SetLayout(Layout);

	Canvas->AddChild(Widget);
	return Widget;
}

SWidget* CreateWidgetsFromJson(const FString& JsonSource)
{
	json Json = json::parse(JsonSource, nullptr, false);
	if (Json.is_discarded() || !Json.is_object())
	{
		return nullptr;
	}

	SCanvasWidget* RootCanvas = new SCanvasWidget(RootCanvasID);
	FWidgetLayout RootLayout;
	RootLayout.AnchorMax = FVector2(1.f, 1.f);
	RootCanvas->SetLayout(RootLayout);

	CreateWidgetsImpl(Json, RootCanvas);
	return RootCanvas;
}

void DestroyWidgets(SWidget* Root)
{
	if (SCanvasWidget* CanvasWidget = dynamic_cast<SCanvasWidget*>(Root))
	{
		for (SWidget* Widget : CanvasWidget->GetChildren())
		{
			DestroyWidgets(Widget);
		}
	}
	delete Root;
}

FWidgetTree::FWidgetTree(SWidget* InRoot)
	: Root(InRoot)
{
	SetWidgetMap(Root);
}

FWidgetTree::~FWidgetTree()
{
	DestroyWidgets(Root);
}

void FWidgetTree::SetWidgetMap(SWidget* Widget)
{
	WidgetsByID.Add(Widget->GetID(), Widget);
	if (SCanvasWidget* Canvas = dynamic_cast<SCanvasWidget*>(Widget))
	{
		for (SWidget* Child : Canvas->GetChildren())
		{
			SetWidgetMap(Child);
		}
	}
}

FWidgetTree* FWidgetTree::CreateFromJson(const FString& JsonSource)
{
	SWidget* Root = CreateWidgetsFromJson(JsonSource);
	return Root ? new FWidgetTree(Root) : nullptr;
}


SWidget* FWidgetTree::GetRootWidget() const
{
	return Root;
}

SWidget* FWidgetTree::FindWidget(FWidgetID ID) const
{
	return WidgetsByID.FindRef(ID);
}
