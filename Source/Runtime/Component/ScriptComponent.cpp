#include "EnginePCH.h"
#include "ScriptComponent.h"

#include "Core/EngineLog.h"
#include "GameFramework/Actor.h"
#include "Script/LuaScriptSystem.h"

UScriptComponent::UScriptComponent()
{
    PrimaryComponentTick.bCanEverTick = true;  //  World의 Tick 목록에 등록
}

void UScriptComponent::InitializeComponent()
{
    Super::InitializeComponent();

    AActor* Owner = GetOwner();
    FLuaScriptSystem& ScriptSystem = FLuaScriptSystem::Get();

    ScriptInstance.Load(ScriptSystem.GetLuaState(), Owner, ScriptPath);
    ScriptInstance.Init();
}

void UScriptComponent::BeginPlay()
{
    Super::BeginPlay();

    ScriptInstance.BeginPlay();
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!IsActive())
    {
        return;
    }

    ScriptInstance.Tick(FLuaScriptSystem::Get().GetLuaState(), DeltaTime);
}

void UScriptComponent::EndPlay()
{
    ScriptInstance.EndPlay();
    Super::EndPlay();
}

void UScriptComponent::SetScriptPath( const FString& InScriptPath)
{
    ScriptPath = InScriptPath;
}

const FString& UScriptComponent::GetScriptPath() const
{
    return ScriptPath;
}

void UScriptComponent::HanldeOnCollisionEnter(UPrimitiveComponent* Other)
{
    ScriptInstance.OnComponentBeginOverlap(Other);
}

void UScriptComponent::HanldeOnCollisionExit(UPrimitiveComponent* Other)
{
    ScriptInstance.OnComponentEndOverlap(Other);
}

void UScriptComponent::HanldeOnCollisionHit(UPrimitiveComponent* Other, FHitResult& HitResult, bool bSelfMoved)
{
    ScriptInstance.OnComponentHit(Other, HitResult, bSelfMoved);
}
