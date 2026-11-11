#include "EnginePCH.h"
#include "LuaScriptInstance.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Actor/PlayerActor.h"
#include "GameFramework/Actor/EnemyActor.h"
#include "GameFramework/Actor/LayerCharacter.h"

void FLuaScriptInstance::Load(sol::state& LuaState, AActor* InOwner, const FString& InScriptPath)
{
    bLoaded = false;
    bHasBegunPlay = false;

    InitFunction = {};
    BeginPlayFunction = {};
    TickFunction = {};
    EndPlayFunction = {};
    OnAnimNotifyFunc = {};
    OnAnimNotifyStateFunc = {};

    Owner = InOwner;
    ScriptPath = InScriptPath;
    State = LuaState.create_table();

    if (!Owner || ScriptPath.empty())
    {
        return;
    }

    // 새로운 environment를 만들고 없는 값을 공용 전역에서 찾음
    Environment = sol::environment(LuaState, sol::create, LuaState.globals());

    if (APlayerActor* Player = Cast<APlayerActor>(Owner))
    {
        Environment["owner"] = Player;
    }
    else if (ALayerCharacter* LayerChar = Cast<ALayerCharacter>(Owner))
    {
        Environment["owner"] = LayerChar;
    }
    else if (AEnemyActor* Enemy = Cast<AEnemyActor>(Owner))
    {
        Environment["owner"] = Enemy;
    }
    else
    {
        Environment["owner"] = Owner;
    }

    Environment["state"] = State;

    sol::protected_function_result Result = LuaState.safe_script_file(ScriptPath, Environment);
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] Load failed: {}, Error: {}", ScriptPath, e.what());
        return;
    }

    InitFunction = Environment["Init"];
    BeginPlayFunction = Environment["BeginPlay"];
    TickFunction = Environment["Tick"];
    EndPlayFunction = Environment["EndPlay"];
    OnComponentBeginOverlapFunc = Environment["OnComponentBeginOverlap"];
    OnComponentEndOverlapFunc = Environment["OnComponentEndOverlap"];
    OnComponentHitFunc = Environment["OnComponentHit"];
    OnAnimNotifyFunc = Environment["OnAnimNotify"];
    OnAnimNotifyStateFunc = Environment["OnAnimNotifyState"];

    bLoaded = true;
    bHasBegunPlay = false;
    LastWriteTime = fs::last_write_time(ScriptPath);
}

void FLuaScriptInstance::Reload(sol::state& LuaState)
{
    if (!State.valid())
    {
        State = LuaState.create_table();
    }

    sol::environment NewEnvironment(LuaState, sol::create, LuaState.globals());
    if (APlayerActor* Player = Cast<APlayerActor>(Owner))
    {
        NewEnvironment["owner"] = Player;
    }
    else if (ALayerCharacter* LayerChar = Cast<ALayerCharacter>(Owner))
    {
        NewEnvironment["owner"] = LayerChar;
    }
    else if (AEnemyActor* Enemy = Cast<AEnemyActor>(Owner))
    {
        NewEnvironment["owner"] = Enemy;
    }
    else
    {
        NewEnvironment["owner"] = Owner;
    }
    NewEnvironment["state"] = State;

    sol::protected_function_result Result = LuaState.safe_script_file(ScriptPath, NewEnvironment);

    if (!Result.valid())
    {
        sol::error Error = Result;
        HTR_LOG(Error, "[Lua] Reload failed: {}, Error: {}", ScriptPath, Error.what());
        return;
    }

    Environment = std::move(NewEnvironment);
    InitFunction = Environment["Init"];
    BeginPlayFunction = Environment["BeginPlay"];
    TickFunction = Environment["Tick"];
    EndPlayFunction = Environment["EndPlay"];
    OnComponentBeginOverlapFunc = Environment["OnComponentBeginOverlap"];
    OnComponentEndOverlapFunc = Environment["OnComponentEndOverlap"];
    OnComponentHitFunc = Environment["OnComponentHit"];
    OnAnimNotifyFunc = Environment["OnAnimNotify"];
    OnAnimNotifyStateFunc = Environment["OnAnimNotifyState"];

    Init();
}

void FLuaScriptInstance::Init()
{
    if (!bLoaded)
    {
        return;
    }

    sol::protected_function_result Result = InitFunction();
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] Init failed: {}, Error: {}", ScriptPath, e.what());
    }
}

void FLuaScriptInstance::BeginPlay()
{
    if (!bLoaded || bHasBegunPlay)
    {
        return;
    }
    bHasBegunPlay = true;

    sol::protected_function_result Result = BeginPlayFunction();
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] BeginPlay failed: {}, Error: {}", ScriptPath, e.what());
    }
}

void FLuaScriptInstance::Tick(sol::state& LuaState, float DeltaTime)
{
    if (!bLoaded || !bHasBegunPlay)
    {
        return;
    }

    sol::protected_function_result Result = TickFunction(DeltaTime);
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] Tick failed: {}, Error: {}", ScriptPath, e.what());
    }

    HotReloadCheckTimer += DeltaTime;

    if (HotReloadCheckTimer >= 0.5f)
    {
        HotReloadCheckTimer = 0.0f;

        std::error_code Error;
        const fs::file_time_type CurrentWriteTime = fs::last_write_time(ScriptPath, Error);

        if (Error)
        {
            return;
        }

        if (CurrentWriteTime != LastWriteTime)
        {
            LastWriteTime = CurrentWriteTime;
            Reload(LuaState);
        }
    }
}

void FLuaScriptInstance::EndPlay()
{
    if (!bHasBegunPlay)
    {
        return;
    }
    bHasBegunPlay = false;

    sol::protected_function_result Result = EndPlayFunction();
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] EndPlay failed: {}, Error: {}", ScriptPath, e.what());
    }
}

void FLuaScriptInstance::OnComponentBeginOverlap(UPrimitiveComponent* Other)
{
    if (!OnComponentBeginOverlapFunc)
    {
        return;
    }

    sol::protected_function_result Result = OnComponentBeginOverlapFunc(Other);
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] EndPlay failed: {}, Error: {}", ScriptPath, e.what());
    }
}

void FLuaScriptInstance::OnComponentEndOverlap(UPrimitiveComponent* Other)
{
    if (!OnComponentEndOverlapFunc)
    {
        return;
    }

    sol::protected_function_result Result = OnComponentEndOverlapFunc(Other);
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] EndPlay failed: {}, Error: {}", ScriptPath, e.what());
    }
}

void FLuaScriptInstance::OnComponentHit(UPrimitiveComponent* Other, FHitResult& HitResult, bool bSelfMoved)
{
    if (!OnComponentHitFunc)
    {
        return;
    }

    sol::protected_function_result Result = OnComponentHitFunc(Other, HitResult, bSelfMoved);
    if (!Result.valid())
    {
        sol::error e = Result;
        HTR_LOG(Error, "[Lua] EndPlay failed: {}, Error: {}", ScriptPath, e.what());
    }
}

void FLuaScriptInstance::OnAnimNotify(const FName& NotifyName)
{
    if (!OnAnimNotifyFunc)
    {
        return;
    }

    const FString Name = NotifyName.ToString();
    sol::protected_function_result Result = OnAnimNotifyFunc(Name);

    if (!Result.valid())
    {
        sol::error Error = Result;
        HTR_LOG(Error, "[Lua] OnAnimNotify failed: {}, Error: {}", ScriptPath, Error.what());
    }
}

void FLuaScriptInstance::OnAnimNotifyState(const FName& NotifyName, EAnimNotifyEventType EventType, float DeltaSeconds)
{
    if (!OnAnimNotifyStateFunc)
    {
        return;
    }

    const char* EventTypeName = "None";

    switch (EventType)
    {
    case EAnimNotifyEventType::Trigger:
        EventTypeName = "Trigger";
        break;
    case EAnimNotifyEventType::Begin:
        EventTypeName = "Begin";
        break;
    case EAnimNotifyEventType::Tick:
        EventTypeName = "Tick";
        break;
    case EAnimNotifyEventType::End:
        EventTypeName = "End";
        break;
    }

    const FString Name = NotifyName.ToString();
    sol::protected_function_result Result = OnAnimNotifyStateFunc(Name, EventTypeName, DeltaSeconds);

    if (!Result.valid())
    {
        sol::error Error = Result;
        HTR_LOG(Error, "[Lua] OnAnimNotifyState failed: {}, Error: {}", ScriptPath, Error.what());
    }
}
