#pragma once

#include "GameFramework/Actor.h"
#include "Component/ScriptComponent.h"

class FLuaScriptEditor
{
public:
	static UScriptComponent* CreateScript(AActor* Actor, FName Name);
	static bool CreateScriptFileAndOpen();

	static void EditScript(const FString& ScriptPath);
	static void EditScript(UScriptComponent* ScriptComponent);
};