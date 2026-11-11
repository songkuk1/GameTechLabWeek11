#pragma once

#include "GameFramework/Actor.h"
#include "Coin.generated.h"

class UCoinMovementComponent;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class ACoin : public AActor
{
	GENERATED_BODY()

public:
	ACoin();

	UCoinMovementComponent* GetCoinMovementComponent() const { return CoinMovementComponent; }

private:
	USphereComponent* CollisionComponent = nullptr;
	UStaticMeshComponent* MeshComponent = nullptr;
	UCoinMovementComponent* CoinMovementComponent = nullptr;
};
