#include "EnginePCH.h"
#include "Coin.h"

#include "CoinMovementComponent.h"
#include "Asset/AssetManager.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"

ACoin::ACoin()
{
	CollisionComponent = CreateDefaultSubobject<USphereComponent>("CoinCollision");
	CollisionComponent->SetBlockComponent(false);

	// DefaultRootComponent 삭제
	USceneComponent* DefaultRoot = GetRootComponent();
	SetRootComponent(CollisionComponent);
	DestroyComponent(DefaultRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Models/Coin/Coin.obj"));

	CoinMovementComponent = CreateDefaultSubobject<UCoinMovementComponent>("CoinMovementComponent");
	CoinMovementComponent->Velocity = FVector::ZeroVector;
	CoinMovementComponent->bShouldBounce = true;
	CoinMovementComponent->Bounciness = 0.6f;
	CoinMovementComponent->bSweepCollision = true;
}
