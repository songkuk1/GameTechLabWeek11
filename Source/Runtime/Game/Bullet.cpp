#include "EnginePCH.h"
#include "Bullet.h"
#include "Asset/AssetManager.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "Component/ScriptComponent.h"

ABullet::ABullet()
{
	CollisionComponent = CreateDefaultSubobject<USphereComponent>("EnemyProjectileCollision");
	CollisionComponent->SetBlockComponent(false);
    CollisionComponent->SetRelativeScale3D(FVector(MeshScale, MeshScale, MeshScale));

	// DefaultRootComponent 삭제
	USceneComponent* DefaultRoot = GetRootComponent();
	SetRootComponent(CollisionComponent);
	DestroyComponent(DefaultRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Models/SpikeBall/SpikeBall.obj"));
    MeshComponent->SetRelativeScale3D(FVector{5.0f, 5.0f, 5.0f});

	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovementComponent");
    MovementComponent->bInitialVelocityInLocalSpace = false;
    MovementComponent->ProjectileGravityScale = 0.0f;
    MovementComponent->MaxSpeed = 10.0f;
    MovementComponent->Velocity = FVector::ZeroVector;

    ScriptComponent = CreateDefaultSubobject<UScriptComponent>("ProjectileScript");
    ScriptComponent->SetScriptPath("Assets/Scripts/Actors/ProjectileActor.lua");
}

void ABullet::SetTarget(AActor* Target)
{
    FVector ForwardDirection = Target->GetActorLocation() - GetActorLocation();
    if (ForwardDirection.SizeSquared() <= SMALL_NUMBER)
    {
        return;
    }
    ForwardDirection = ForwardDirection.GetSafeNormal();

    const float SpawnYaw = std::atan2(ForwardDirection.Y, ForwardDirection.X) * 180.0f / PI;
    SetActorRotation(FRotator(0.0f, SpawnYaw, 0.0f));
    MovementComponent->Velocity = ForwardDirection * 10.0f;
}

void ABullet::SetDirection(const FVector& Direction, float Speed)
{
    if (Direction.SizeSquared() <= SMALL_NUMBER)
    {
        return;
    }

    const FVector NormalizedDirection = Direction.GetSafeNormal();

    const float SpawnYaw = std::atan2(NormalizedDirection.Y, NormalizedDirection.X) * 180.0f / PI;

    SetActorRotation(FRotator(0.0f, SpawnYaw, 0.0f));
    MovementComponent->Velocity = NormalizedDirection * Speed;
}

