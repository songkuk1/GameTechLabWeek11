#include "EnginePCH.h"
#include "PlayerActor.h"

#include "Engine/World.h"
#include "Asset/AssetManager.h"
#include "Component/BoxComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/SphereComponent.h"
#include "Component/BoxComponent.h"

#include "Game/CoinMovementComponent.h"
#include "Game/Coin.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "Camera/CameraShakeHit.h"
#include "Camera/PlayerCameraManager.h"

APlayerActor::APlayerActor()
{
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("USpringArmComponent");
	SpringArmComponent->SetupAttachment(GetRootComponent());
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("UCameraComponent");
	CameraComponent->SetupAttachment(SpringArmComponent);
    bUseControllerRotationYaw = false;

    PrimaryActorTick.bCanEverTick = true;
    ScriptComponent = CreateDefaultSubobject<UScriptComponent>("UScriptComponent");
    ScriptComponent->SetScriptPath("Assets/Scripts/Actors/PlayerActor.lua");

    CharacterMovementComponent = CreateDefaultSubobject<UCharacterMovementComponent>("UCharacterMovementComponent");
    CharacterMovementComponent->Gravity = 20.0f;

    PlayerMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("PlayerMeshComponent");
    PlayerMeshComponent->SetupAttachment(GetRootComponent());

    WandMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("WandMeshComponent");
    WandMeshComponent->SetupAttachment(PlayerMeshComponent);
    
    BoxComponent = CreateDefaultSubobject<UBoxComponent>("PlayerCollision");
    BoxComponent->SetupAttachment(GetRootComponent());
    BoxComponent->SetBlockComponent(true);
    CharacterMovementComponent->SetCollisionComponent(BoxComponent);

    MagnetCollision = CreateDefaultSubobject<USphereComponent>("MagnetCollision");

    MagnetCollision->SetupAttachment(GetRootComponent());
    MagnetCollision->SetBlockComponent(false);
    MagnetCollision->SetNotifyScriptCollision(false);
    MagnetCollision->SetSphereRadius(MagnetRadius);
}

void APlayerActor::StartWandSwing()
{
    bWandSwinging = true;
    WandSwingElapsed = 0.0f;
    WandStartRotation = WandMeshComponent->GetRelativeRotation();
}

void APlayerActor::UpdateWandSwing(float RealDeltaTime)
{
    WandSwingElapsed += RealDeltaTime;

    const float Alpha = FMath::Clamp(WandSwingElapsed / WandSwingDuration, 0.0f, 1.0f);
    const float SwingAlpha = std::sin(Alpha * PI);

    FRotator Rotation = WandStartRotation;
    Rotation.Pitch += SwingAlpha * WandSwingAngle;
    WandMeshComponent->SetRelativeRotation(Rotation);
    if (Alpha >= 1.0f)
    {
        WandMeshComponent->SetRelativeRotation(WandStartRotation);
        bWandSwinging = false;
    }
}

void APlayerActor::BeginPlay()
{
    Super::BeginPlay();

    WandMeshComponent = Cast<UStaticMeshComponent>(FindComponentByName("WandMeshComponent"));
}

void APlayerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    DamageEffectIntensity = std::max(0.0f, DamageEffectIntensity - DeltaTime * 2.0f);

    if (bDying)
    {
        UpdateDeathFall(DeltaTime);
    }

    if (bWandSwinging)
    {
        UpdateWandSwing(DeltaTime);
    }

    for (UPrimitiveComponent* Other : MagnetCollision->GetOverlapComps())
    {
        if (!Other || Other->GetName() != "CoinCollision")
        {
            continue;
        }

        ACoin* Coin = Cast<ACoin>(Other->GetOwner());
        if (!Coin)
        {
            continue;
        }

        UCoinMovementComponent* Movement = Coin->GetCoinMovementComponent();

        FVector TargetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 0.5f);
        FVector ToPlayer = TargetLocation - Coin->GetActorLocation();

        const float Distance = ToPlayer.Size();
        if (Distance <= SMALL_NUMBER)
        {
            continue;
        }

        const FVector Direction = ToPlayer / Distance;

        // 바닥에서 정지한 코인도 다시 움직이도록 깨움
        Movement->bSimulationEnabled = true;
        Movement->MaxSpeed = MagnetMaxSpeed;

        // 현재 속도를 원하는 속도로 부드럽게 유도
        const FVector DesiredVelocity = Direction * MagnetMaxSpeed;
        const FVector Steering = (DesiredVelocity - Movement->Velocity) * MagnetAcceleration;
        Movement->AddForce(Steering);
    }
}

// 입력을 시선 수평 방향으로 변환하여 이동
void APlayerActor::Move(const FVector& MoveInput)
{
	const FQuat YawQuat = FRotator(0.0f, GetControlRotation().Yaw, 0.0f).Quaternion();
    CharacterMovementComponent->SetMoveInput(YawQuat.RotateVector(MoveInput));
}

void APlayerActor::Jump(float JumpSpeed)
{
    CharacterMovementComponent->Jump(JumpSpeed);
}

UCharacterMovementComponent* APlayerActor::GetCharacterMovementComponent()
{
    return CharacterMovementComponent;
}

void APlayerActor::SetMagnetRadius(float InMagnetRadius)
{
    MagnetRadius = std::max(0.0f, InMagnetRadius);

    if (MagnetCollision)
    {
        MagnetCollision->SetSphereRadius(MagnetRadius);
    }
}

void APlayerActor::PlayerProjectile()
{
    const FVector Direction = GetControlRotation()
        .Quaternion()
        .RotateVector(FVector(1.0f, 0.0f, 0.0f))
        .GetSafeNormal();

    const FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 1.5f) + Direction * 2.0f;
    const FTransform SpawnTransform(SpawnLocation, FVector(1.0), GetControlRotation());

    AActor* Projectile = World->SpawnActor<AActor>(NAME_None, &SpawnTransform);
    if (!Projectile)
    {
        return;
    }

    UStaticMeshComponent* PlayerProjectileMesh =
        Cast<UStaticMeshComponent>(Projectile->AddComponent(UStaticMeshComponent::StaticClass(), FName("PlayerProjectileMesh")));
    if (UStaticMesh* CubeMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Models/CubeBolt/CubeBolt.obj"))
    {
        PlayerProjectileMesh->SetStaticMesh(CubeMesh);
        PlayerProjectileMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    }


    UBoxComponent* PlayerProjectileCollision =
        Cast<UBoxComponent>(Projectile->AddComponent(UBoxComponent::StaticClass(),  FName("PlayerProjectileCollision")));
    UProjectileMovementComponent* PlayerProjectileMovement =
        Cast<UProjectileMovementComponent>(Projectile->AddComponent(UProjectileMovementComponent::StaticClass(), FName("PlayerProjectileMovement")));
    UScriptComponent* PlayerProjectileScript =
        Cast<UScriptComponent>(Projectile->AddComponent(UScriptComponent::StaticClass(), FName("PlayerProjectileScript")));
    PlayerProjectileScript->SetScriptPath("Assets/Scripts/Actors/PlayerProjectileActor.lua");

    const FVector ProjectileScale(0.9f, 0.9f, 0.9f);
    PlayerProjectileMesh->SetRelativeScale3D(ProjectileScale);
    UMaterial* Material = UMaterial::CreateInstance(PlayerProjectileMesh->GetMaterial(0));
    Material->BaseColor = FLinearColor(0.0f, 0.0f, 7.0f, 1.0f);
    PlayerProjectileMesh->SetMaterial(0, Material);

    PlayerProjectileMovement->bInitialVelocityInLocalSpace = false;
    PlayerProjectileMovement->ProjectileGravityScale = 0.0f;
    PlayerProjectileMovement->MaxSpeed = 15.0f;
    PlayerProjectileMovement->Velocity = Direction * 15.0f;
}

void APlayerActor::PlayDamageEffect()
{
    DamageEffectIntensity = 1.0f;

    // 테스트: 피격 카메라 흔들림
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetPlayerController())
        {
            if (APlayerCameraManager* PCM = PC->GetPlayerCameraManager())
            {
                PCM->StartCameraShake<UCameraShakeHit>();
            }
        }
    }
}

float APlayerActor::GetDamageEffectIntensity() const
{
    return DamageEffectIntensity;
}

void APlayerActor::ApplyKnockback(AActor* DamageSource, float Strength)
{
    FVector Direction = GetActorLocation() - DamageSource->GetActorLocation();

    Direction.Z = 0.4f;
    if (Direction.SizeSquared() <= SMALL_NUMBER)
    {
        return;
    }
    Direction = Direction.GetSafeNormal();

    CharacterMovementComponent->AddKnockback(Direction * Strength);
}

void APlayerActor::StartDeathFall()
{
    bDying = true;
    DeathFallElapsed = 0.0f;
    DeathStartRotation = PlayerMeshComponent->GetRelativeRotation();
    DeathStartLocation = PlayerMeshComponent->GetRelativeLocation();

    bWandSwinging = false;
    WandMeshComponent->SetRelativeRotation(WandStartRotation);
}

void APlayerActor::UpdateDeathFall(float RealDeltaTime)
{
    DeathFallElapsed += RealDeltaTime;

    const float Alpha = FMath::Clamp(DeathFallElapsed / DeathFallDuration, 0.0f, 1.0f);
    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
    const float TargetPitch = DeathStartRotation.Pitch + 90.0f;

    FRotator DeathFallPitchRotation = DeathStartRotation;
    DeathFallPitchRotation.Pitch = DeathStartRotation.Pitch + (TargetPitch - DeathStartRotation.Pitch) * SmoothAlpha;
    PlayerMeshComponent->SetRelativeRotation(DeathFallPitchRotation);

    const float JumpAlpha = std::sin(Alpha * PI);
    FVector DeathLocation = DeathStartLocation;
    float FallenHeightOffset = 0.4f;

    DeathLocation.Z += JumpAlpha * DeathJumpHeight + SmoothAlpha * FallenHeightOffset;
    PlayerMeshComponent->SetRelativeLocation(DeathLocation);

    if (Alpha >= 1.0f)
    {
        bDying = false;
    }
}

