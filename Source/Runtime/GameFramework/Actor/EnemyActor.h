#pragma once

#include "GameFramework/Pawn.h"
#include "Component/StaticMeshComponent.h"
#include "Component/CharacterMovementComponent.h"
#include "EnemyActor.generated.h"

class UScriptComponent;
class UBoxComponent;

UCLASS()
class AEnemyActor : public APawn
{
    GENERATED_BODY()


public:
    AEnemyActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void LookAtPlayer();
    void Jump(float JumpSpeed);
    void MoveToward(const FVector& Direction, float InSpeed);
    void EnemyProjectileAtPlayer();

    UCharacterMovementComponent* GetCharacterMovementComponent();

    void PlayHitSquash();
    void PlaySlamCameraShake();
    bool IsGrounded() const;

private:
    UScriptComponent* ScriptComponent = nullptr;
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UCharacterMovementComponent* CharacterMovementComponent = nullptr;
    UBoxComponent* BoxComponent = nullptr;
    APawn* Player = nullptr;

    float JumpSpeed = 7.0f;

    bool bHitSquashing = false;
    float HitSquashElapsed = 0.0f;
    float HitSquashDuration = 0.15f;
    FVector OriginalMeshScale = FVector::OneVector;
};