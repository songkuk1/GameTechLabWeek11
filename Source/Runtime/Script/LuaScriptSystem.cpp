#include "EnginePCH.h"

#include "LuaScriptSystem.h"
#include "LuaBindings.h"
#include "LuaCoroutineScheduler.h"

FLuaScriptSystem& FLuaScriptSystem::Get()
{
    static FLuaScriptSystem Instance;
    return Instance;
}

void FLuaScriptSystem::Initialize()
{
    if (bInitialized)
    {
        return;
    }

    LuaState.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string,
		sol::lib::coroutine
    );

    CoroutineScheduler = MakeUnique<FLuaCoroutineScheduler>(LuaState);

    FLuaBindings::RegisterLuaBindings(LuaState, CoroutineScheduler.get());
    bInitialized = true;
}

void FLuaScriptSystem::Update(float DeltaTime)
{
    if (CoroutineScheduler)
    {
        CoroutineScheduler->Update(DeltaTime);
    }
}

void FLuaScriptSystem::Shutdown()
{
    CoroutineScheduler.reset();
    bInitialized = false;
}

void FLuaScriptSystem::StopAllCoroutines()
{
    if (CoroutineScheduler)
    {
        CoroutineScheduler->StopAllCoroutines();
    }
}

sol::state& FLuaScriptSystem::GetLuaState()
{
    return LuaState;
}