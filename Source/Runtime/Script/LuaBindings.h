#pragma once

#include "LuaScriptSystem.h"
#include "UI/WidgetLayout.h"

class FLuaCoroutineScheduler;
class AActor;
class UWorld;
class UClass;
class FWidgetTree;
struct FTransform;

class FLuaBindings
{
public:
	static void RegisterLuaBindings(sol::state& Lua, FLuaCoroutineScheduler* CoroutinScheduler);

private:
	static sol::object FindComponentByName(const AActor& Actor, sol::string_view NameStr);
	static sol::object FindComponentByClass(const AActor& Actor, const UClass* Clss);
	static sol::object SpawnActor(UWorld& World, UClass* Class, const FTransform* Transform);
	static sol::object SpawnActor(UWorld& World, UClass* Class, sol::string_view NameStr, const FTransform* Transform);

	static void OpenLevel(AActor* WorldContextObject, sol::string_view NextURL);
	static AActor* GetActorOfClass(AActor* WorldContextObject, const UClass* Clss);
	static AActor* GetActorByName(AActor* WorldContextObject, sol::string_view Name);

	static FWidgetTree* CreateWidgetTree(sol::string_view JsonSource);
	static void DetroyWidgetTree(FWidgetTree* Tree);
	static sol::object FindWidget(FWidgetTree& Tree, FWidgetID ID);
	static void AddToViewport(FWidgetTree* Tree);
	static void RemoveFromViewport(FWidgetTree* Tree);
};
