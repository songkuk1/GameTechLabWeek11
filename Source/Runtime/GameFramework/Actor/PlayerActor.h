#pragma once

#include "GameFramework/Pawn.h"
#include "Component/StaticMeshComponent.h"
#include "Component/CharacterMovementComponent.h"

class UCameraComponent;
class USpringArmComponent;
class UScriptComponent;
class USphereComponent;
class UBoxComponent;

class APlayerActor : public APawn
{
    DECLARE_CLASS(APlayerActor, APawn)

    REFLECT_START(ClassName)
        REFLECT_END()

public:
    APlayerActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void Move(const FVector& MoveInput);
    void Jump(float JumpSpeed);

    UCharacterMovementComponent* GetCharacterMovementComponent();
    void SetMagnetRadius(float InMagnetRadius);

    void PlayerProjectile();
    void StartWandSwing();
    void UpdateWandSwing(float RealDeltaTime);

    void PlayDamageEffect();
    float GetDamageEffectIntensity() const;
    void ApplyKnockback(AActor* DamageSource, float Strength);
    void StartDeathFall();
    void UpdateDeathFall(float RealDeltaTime);

private:
	USpringArmComponent* SpringArmComponent = nullptr;
	UCameraComponent* CameraComponent = nullptr;
    UScriptComponent* ScriptComponent = nullptr;
    UStaticMeshComponent* PlayerMeshComponent = nullptr;
    UStaticMeshComponent* WandMeshComponent = nullptr;
    UCharacterMovementComponent* CharacterMovementComponent = nullptr;
    UBoxComponent* BoxComponent = nullptr;

    float DamageEffectIntensity = 0.0f;
    float JumpSpeed = 7.0f;


    bool bDying = false;
    float DeathFallElapsed = 0.0f;
    float DeathFallDuration = 0.3f;
    FRotator DeathStartRotation = { 0.f, 0.f, 0.f };
    FVector DeathStartLocation = FVector::ZeroVector;
    float DeathJumpHeight = 1.5f;

    bool bWandSwinging = false;
    float WandSwingElapsed = 0.0f;
    float WandSwingDuration = 0.2f;
    float WandSwingAngle = 50.0f;
    FRotator WandStartRotation = { 0.f, 0.f, 0.f };

    USphereComponent* MagnetCollision = nullptr;
    float MagnetRadius = 6.0f;
    float MagnetAcceleration = 30.0f;
    float MagnetMaxSpeed = 15.0f;
};