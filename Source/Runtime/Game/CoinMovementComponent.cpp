#include "EnginePCH.h"
#include "CoinMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr float SpinDeceleration = 150.0f;
	constexpr float SpinPerImpactSpeed = 20.0f;
}

bool UCoinMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!Primitive)
	{
		return Super::MoveUpdatedComponent(Delta, NewRotation, bSweep, Hit);
	}

	// 코인은 기본적으로 bBlockComponent가 false인데, 이동 중에만 잠깐 켜서 충돌을 감지하도록 한다. 다른 코인은 false 상태이므로 충돌하지 않는다.
	Primitive->SetBlockComponent(true);
	const bool bMoved = Super::MoveUpdatedComponent(Delta, NewRotation, bSweep, Hit);
	Primitive->SetBlockComponent(false);
	return bMoved;
}

void UCoinMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!UpdatedComponent || SpinRate == 0.0f)
	{
		return;
	}

	FRotator Rotation = UpdatedComponent->GetRelativeRotation();
	Rotation.Yaw = std::fmod(Rotation.Yaw + SpinRate * DeltaTime, 360.0f);
	UpdatedComponent->SetRelativeRotation(Rotation);

	const float Deceleration = SpinDeceleration * DeltaTime;
	SpinRate = std::fabs(SpinRate) <= Deceleration ? 0.0f : SpinRate - std::copysign(Deceleration, SpinRate);
}

void UCoinMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	AActor* HitOwner = Hit.HitComponent
		? Hit.HitComponent->GetOwner()
		: nullptr;

	const bool bHitEnemy = Hit.HitComponent && Hit.HitComponent->GetName() == "EnemyCollision";

	if (Cast<APawn>(HitOwner) && !bHitEnemy)
	{
		return;
	}

	const float ImpactSpeed = std::max(0.0f, -Velocity.Dot(Hit.Normal));
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
	if (ImpactSpeed > 1.0f)
	{
		SpinRate += std::copysign(ImpactSpeed, SpinRate) * SpinPerImpactSpeed;
	}

	if (bHitEnemy)
	{
		FVector PushDirection = UpdatedComponent->GetWorldLocation() - HitOwner->GetActorLocation();
		PushDirection.Z = 0.0f;
		PushDirection = PushDirection.GetSafeNormal();

		Velocity += PushDirection * 1.5f;
	}
}
