#include "EnginePCH.h"
#include "EnemyActor.h"

#include "Engine/World.h"
#include "Asset/AssetManager.h"
#include "Component/ScriptComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "Component/SphereComponent.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "Camera/CameraShakeSlam.h"
#include "Camera/PlayerCameraManager.h"

AEnemyActor::AEnemyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    ScriptComponent = CreateDefaultSubobject<UScriptComponent>("UScriptComponent");
    ScriptComponent->SetScriptPath("Assets/Scripts/Actors/EnemyActor.lua");

    CharacterMovementComponent = CreateDefaultSubobject<UCharacterMovementComponent>("UCharacterMovementComponent");

    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("UStaticMeshComponent");
    StaticMeshComponent->SetupAttachment(GetRootComponent());

    SphereComponent = CreateDefaultSubobject<USphereComponent>("EnemyCollision");
    SphereComponent->SetupAttachment(GetRootComponent());
    CharacterMovementComponent->SetCollisionComponent(SphereComponent);
}

void AEnemyActor::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    OriginalMeshScale = StaticMeshComponent->GetRelativeScale3D();

    APlayerController* PlayerController = World->GetPlayerController();
    Player = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Player)
    {
        return;
    }
}

void AEnemyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bHitSquashing)
    {
        HitSquashElapsed += DeltaTime;
        const float Ratio = FMath::Clamp(HitSquashElapsed / HitSquashDuration, 0.0f, 1.0f);
        const float Amount = std::sin(Ratio * PI);

        const float XYScale = 1.0f + Amount * 0.15f;
        const float ZScale = 1.0f - Amount * 0.30f;

        const FVector SquashScale = OriginalMeshScale * FVector(XYScale, XYScale, ZScale);
        StaticMeshComponent->SetRelativeScale3D(SquashScale);

        if (Ratio >= 1.0f)
        {
            StaticMeshComponent->SetRelativeScale3D(OriginalMeshScale);
            HitSquashElapsed = 0.0f;
            bHitSquashing = false;
        }
    }

    if (!Player)
    {
        return;
    }

    LookAtPlayer();
}

void AEnemyActor::LookAtPlayer()
{
    FVector Direction = Player->GetActorLocation() - GetActorLocation();
    Direction.Z = 0.0f;
    if (Direction.SizeSquared() <= SMALL_NUMBER)
    {
        return;
    }

    FRotator Rotation = GetActorRotation();
    Rotation.Yaw = std::atan2(Direction.Y, Direction.X) * 180.0f / PI - 180;
    SetActorRotation(Rotation);
}

void AEnemyActor::Jump(float JumpSpeed)
{
    CharacterMovementComponent->Jump(JumpSpeed);
}

void AEnemyActor::EnemyProjectileAtPlayer()
{
    if (!Player)
    {
        return;
    }

    FVector ForwardDirection = Player->GetActorLocation() - GetActorLocation();
    ForwardDirection.Z = 0.0f;
    if (ForwardDirection.SizeSquared() <= SMALL_NUMBER)
    {
        return;
    }
    ForwardDirection = ForwardDirection.GetSafeNormal();

    const float ForwardOffset = 3.0f;
    const float HeightOffset = 1.5f;
    const float MeshScale = 0.4f;

    const FVector SpawnLocation = GetActorLocation() + ForwardDirection * ForwardOffset + FVector(0.0f, 0.0f, HeightOffset);
    const FVector TargetLocation = Player->GetActorLocation() + FVector(0.0f, 0.0f, 1.0f);

    const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal();
    const float SpawnYaw = std::atan2(Direction.Y, Direction.X) * 180.0f / PI;

    const FTransform SpawnTransform(SpawnLocation, FVector(1.0f), FRotator(0.0f, SpawnYaw, 0.0f));

    AActor* Projectile = World->SpawnActor<AActor>(NAME_None, &SpawnTransform);
    if (!Projectile)
    {
        return;
    }

    UStaticMeshComponent* EnemyProjectileMesh = 
        Cast<UStaticMeshComponent>(Projectile->AddComponent(UStaticMeshComponent::StaticClass(), FName("EnemyProjectileMesh")));
    if (UStaticMesh* SphereMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Sphere"))
    {
        EnemyProjectileMesh->SetStaticMesh(SphereMesh);
    }

    UProjectileMovementComponent* EnemyProjectileMovement = 
        Cast<UProjectileMovementComponent>(Projectile->AddComponent(UProjectileMovementComponent::StaticClass(), FName("EnemyProjectileMovement")));
    USphereComponent* EnemyProjectileCollision = 
        Cast<USphereComponent>(Projectile->AddComponent(USphereComponent::StaticClass(), FName("EnemyProjectileCollision")));
    UScriptComponent* EnemyProjectileScript = 
        Cast<UScriptComponent>(Projectile->AddComponent(UScriptComponent::StaticClass(), FName("EnemyProjectileScript")));
    EnemyProjectileScript->SetScriptPath("Assets/Scripts/Actors/EnemyProjectileActor.lua");

    EnemyProjectileMesh->SetRelativeScale3D(FVector(MeshScale, MeshScale, MeshScale));
    UMaterial* Material = UMaterial::CreateInstance(EnemyProjectileMesh->GetMaterial(0));
    Material->BaseColor = FLinearColor(6.0f, 0.0f, 0.0f, 1.0f);
    EnemyProjectileMesh->SetMaterial(0, Material);

    EnemyProjectileMovement->bInitialVelocityInLocalSpace = false;
    EnemyProjectileMovement->ProjectileGravityScale = 0.0f;
    EnemyProjectileMovement->MaxSpeed = 10.0f;
    EnemyProjectileMovement->Velocity = Direction * 10.0f;
}

UCharacterMovementComponent* AEnemyActor::GetCharacterMovementComponent()
{
    return CharacterMovementComponent;
}

void AEnemyActor::PlayHitSquash()
{
    bHitSquashing = true;
    HitSquashElapsed = 0.0f;
}

void AEnemyActor::PlaySlamCameraShake()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (APlayerController* PC = World->GetPlayerController())
    {
        if (APlayerCameraManager* PCM = PC->GetPlayerCameraManager())
        {
            PCM->StartCameraShake<UCameraShakeSlam>();
        }
    }
}

bool AEnemyActor::IsGrounded() const
{
    return CharacterMovementComponent &&  CharacterMovementComponent->IsGrounded();
}