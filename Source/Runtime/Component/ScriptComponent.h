#pragma once

#include "ActorComponent.h"
#include "Script/LuaScriptInstance.h"
#include "ScriptComponent.generated.h"

class UPrimitiveComponent;
enum class EAnimNotifyEventType : uint8;

UCLASS()
class UScriptComponent : public UActorComponent
{
    GENERATED_BODY()


public:
    UScriptComponent();

    void InitializeComponent() override;
    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;
    void EndPlay() override;

    void SetScriptPath(const FString& InScriptPath);
    const FString& GetScriptPath() const;

    void HanldeOnCollisionEnter(UPrimitiveComponent* Other);
    void HanldeOnCollisionExit(UPrimitiveComponent* Other);
    void HanldeOnCollisionHit(UPrimitiveComponent* Other, FHitResult& HitResult, bool bSelfMoved);
    void HandleAnimNotify(const FName& NotifyName);
    void HandleAnimNotifyState(const FName& NotifyName, EAnimNotifyEventType EventType, float DeltaSeconds);

    inline sol::table GetLuaTable()
    {
        return ScriptInstance.GetTable();
    }

private:
    UPROPERTY(EditAnywhere)
    FString ScriptPath = "Assets/Scripts/Templates/Template.lua";
    FLuaScriptInstance ScriptInstance;
};