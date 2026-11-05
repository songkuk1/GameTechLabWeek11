#pragma once

#include "GameFramework/Actor.h"

class UCoinMovementComponent;
class UStaticMeshComponent;
class USphereComponent;

class ACoin : public AActor
{
	DECLARE_CLASS(ACoin, AActor)

public:
	ACoin();

	UCoinMovementComponent* GetCoinMovementComponent() const { return CoinMovementComponent; }

private:
	USphereComponent* CollisionComponent = nullptr;
	UStaticMeshComponent* MeshComponent = nullptr;
	UCoinMovementComponent* CoinMovementComponent = nullptr;
};
