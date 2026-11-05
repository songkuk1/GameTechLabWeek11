#pragma once

#include "WidgetLayout.h"
#include "Containers/Map.h"
#include "Core/EngineString.h"

class SWidget;

class FWidgetTree
{
public:
	FWidgetTree(SWidget* InRoot);
	~FWidgetTree();

	SWidget* GetRootWidget() const;

	SWidget* FindWidget(FWidgetID ID) const;

	static FWidgetTree* CreateFromJson(const FString& JsonSource);

private:
	void SetWidgetMap(SWidget* Widget);

	SWidget* Root = nullptr;
	TMap<FWidgetID, SWidget*> WidgetsByID;
};

SWidget* CreateWidgetsFromJson(const FString& JsonSource);
void DestroyWidgets(SWidget* Root);
