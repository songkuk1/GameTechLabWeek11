#pragma once

#include "GameFramework/Actor.h"
#include "Component/ScriptComponent.h"

class UScriptComponent;

class ALuaTestActor : public AActor
{
    DECLARE_CLASS(ALuaTestActor, AActor)

    REFLECT_START(ClassName)
        REFLECT_END()

public:
    ALuaTestActor();

private:
    UScriptComponent* ScriptComponent = nullptr;
};