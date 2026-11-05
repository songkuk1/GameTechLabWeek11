#pragma once

#include "GameFramework/Actor.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UScriptComponent;

class ABullet : public AActor
{
	DECLARE_CLASS(ABullet, AActor)

public:
	ABullet();

	void SetTarget(AActor* Target);
	void SetDirection(const FVector& Direction, float Speed);

private:
	const float ForwardOffset = 3.0f;
	const float HeightOffset = 1.5f;
	const float MeshScale = 0.4f;

	USphereComponent* CollisionComponent = nullptr;
	UStaticMeshComponent* MeshComponent = nullptr;
	UProjectileMovementComponent* MovementComponent = nullptr;
	UScriptComponent* ScriptComponent = nullptr;
};