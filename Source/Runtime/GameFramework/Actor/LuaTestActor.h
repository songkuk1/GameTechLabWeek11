#pragma once

#include "GameFramework/Actor.h"
#include "Component/ScriptComponent.h"
#include "LuaTestActor.generated.h"

class UScriptComponent;

UCLASS()
class ALuaTestActor : public AActor
{
    GENERATED_BODY()


public:
    ALuaTestActor();

private:
    UScriptComponent* ScriptComponent = nullptr;
};