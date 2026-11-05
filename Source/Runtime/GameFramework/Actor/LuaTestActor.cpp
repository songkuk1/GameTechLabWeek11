#include "EnginePCH.h"
#include "LuaTestActor.h"

#include "Component/ScriptComponent.h"

ALuaTestActor::ALuaTestActor()
{
    ScriptComponent =CreateDefaultSubobject<UScriptComponent>("LuaScriptComponent");
    ScriptComponent->SetScriptPath("Assets/Scripts/Actors/TestActor.lua");
}