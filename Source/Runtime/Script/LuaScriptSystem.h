#pragma once

#include <sol/sol.hpp>
#include "Core/Types.h"

class FLuaCoroutineScheduler;

// Lua 실행 환경 전체를 관리
class FLuaScriptSystem
{
public:
    static FLuaScriptSystem& Get();

    void Initialize();
    void Update(float DeltaTime);
    void Shutdown();
    void StopAllCoroutines();

    sol::state& GetLuaState();

private:
    FLuaScriptSystem() = default;

    sol::state LuaState;
    TUniquePtr<FLuaCoroutineScheduler> CoroutineScheduler;

    bool bInitialized = false;
};