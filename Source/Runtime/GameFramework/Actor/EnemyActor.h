#pragma once

#include "GameFramework/Pawn.h"
#include "Component/StaticMeshComponent.h"
#include "Component/CharacterMovementComponent.h"

class UScriptComponent;
class USphereComponent;

class AEnemyActor : public APawn
{
    DECLARE_CLASS(AEnemyActor, APawn)

    REFLECT_START(ClassName)
        REFLECT_END()

public:
    AEnemyActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void LookAtPlayer();
    void Jump(float JumpSpeed);

    void EnemyProjectileAtPlayer();

    UCharacterMovementComponent* GetCharacterMovementComponent();

    void PlayHitSquash();
    void PlaySlamCameraShake();
    bool IsGrounded() const;

private:
    UScriptComponent* ScriptComponent = nullptr;
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UCharacterMovementComponent* CharacterMovementComponent = nullptr;
    USphereComponent* SphereComponent = nullptr;
    APawn* Player = nullptr;

    float JumpSpeed = 7.0f;

    bool bHitSquashing = false;
    float HitSquashElapsed = 0.0f;
    float HitSquashDuration = 0.15f;
    FVector OriginalMeshScale = FVector::OneVector;
};