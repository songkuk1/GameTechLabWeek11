#include "EnginePCH.h"
#include "CharacterMovementComponent.h"

#include "Engine/World.h"

namespace
{
	float StepAngleTowards(float Current, float Target, float MaxStep)
	{
		float Delta = Target - Current;
		while (Delta > 180.0f)
		{
			Delta -= 360.0f;
		}
		while (Delta < -180.0f)
		{
			Delta += 360.0f;
		}

		if (std::fabs(Delta) < MaxStep)
		{
			return Target;
		}
		return Current + (Delta > 0.0f ? MaxStep : -MaxStep);
	}
}

UCharacterMovementComponent::UCharacterMovementComponent()
{
	PrimaryComponentTick.TickGroup = ETickingGroup::DuringPhysics;
}

void UCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!UpdatedComponent)
	{
		return;
	}

	if (PendingMoveInput.SizeSquared() > 1.0f)
	{
		PendingMoveInput = PendingMoveInput.Normalized();
	}

	Velocity.X = PendingMoveInput.X * MoveSpeed;
	Velocity.Y = PendingMoveInput.Y * MoveSpeed;

	// 바닥 위에 서 있고 위로 뛰는 중이 아니면 중력을 쌓지 않는다.
	// 그 외(공중, 점프 직후)에는 중력을 적용하고, 착지는 아래 MoveWithSlide의 바닥 Hit에서 처리한다.
	if (IsGrounded() && Velocity.Z <= 0.0f)
	{
		Velocity.Z = 0.0f;
	}
	else
	{
		Velocity.Z -= Gravity * DeltaTime;
	}

	const FVector FinalVelocity = Velocity + KnockbackVelocity;
	const FVector MoveDelta = FinalVelocity * DeltaTime;

	FRotator NewRotation = UpdatedComponent->GetRelativeRotation();
	if (bOrientRotationToMovement && (Velocity.X * Velocity.X + Velocity.Y * Velocity.Y) > 1e-4f)
	{
		const float TargetYaw = std::atan2(Velocity.Y, Velocity.X) * 180.0f / PI;
		NewRotation.Yaw = StepAngleTowards(NewRotation.Yaw, TargetYaw, RotationRate * DeltaTime);
	}

	MoveWithSlide(MoveDelta, NewRotation);

	const float CurrentKnockbackSpeed = KnockbackVelocity.Size();
	const float NewKnockbackSpeed = std::max(0.0f, CurrentKnockbackSpeed - KnockbackDeceleration * DeltaTime);

	if (CurrentKnockbackSpeed > SMALL_NUMBER)
	{
		KnockbackVelocity = KnockbackVelocity.GetSafeNormal() * NewKnockbackSpeed;
	}
	else
	{
		KnockbackVelocity = FVector::ZeroVector;
	}

	PendingMoveInput = FVector::ZeroVector;
	UpdateComponentVelocity();
}

void UCharacterMovementComponent::MoveWithSlide(const FVector& Delta, const FRotator& NewRotation)
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;

	// 막힐 콜리전이 없으면 sweep 없이 그대로 이동한다
	if (!World || !CollisionComponent || !CollisionComponent->IsBlockComponent())
	{
		MoveUpdatedComponent(Delta, NewRotation, false, nullptr);
		return;
	}

	FPhysicsScene& PhysicsScene = World->GetPhysicsScene();
	FVector Remaining = Delta;

	for (int32 Iter = 0; Iter < MaxSlideIterations; ++Iter)
	{
		if (Remaining.SizeSquared() < 1e-8f)
		{
			break;
		}

		FHitResult Hit;
		if (!PhysicsScene.ComponentSweepSingle(CollisionComponent, Remaining, NewRotation, Hit))
		{
			// 막히지 않음: 남은 이동을 다 하고 끝
			MoveUpdatedComponent(Remaining, NewRotation, false, nullptr);
			break;
		}

		// 막힌 지점보다 조금 덜 간다
		const float MoveDistance = Remaining.Size();
		const float HitDistance = MoveDistance * Hit.Time;
		const float PullBack = FMath::Min(0.01f, HitDistance);
		MoveUpdatedComponent(Remaining.GetSafeNormal() * (HitDistance - PullBack), NewRotation, false, nullptr);

		PhysicsScene.QueueHit({ CollisionComponent, Hit.HitComponent, Hit, true });
		FHitResult OtherHit = Hit;
		OtherHit.Normal *= -1.0f;
		OtherHit.ImpactNormal *= -1.0f;
		OtherHit.HitComponent = CollisionComponent;
		PhysicsScene.QueueHit({ Hit.HitComponent, CollisionComponent, OtherHit, false });

		const FVector Normal = Hit.Normal;

		// 바닥에 착지: 떨어지던 속도를 없앤다 (다음 프레임부터는 IsGrounded로 중력도 멈춘다)
		if (Normal.Z >= WalkableFloorZ && Velocity.Z < 0.0f)
		{
			Velocity.Z = 0.0f;
		}
		// 천장에 머리를 박음: 올라가던 속도를 없앤다
		else if (Normal.Z <= -WalkableFloorZ && Velocity.Z > 0.0f)
		{
			Velocity.Z = 0.0f;
		}

		// 남은 이동에서 막힌 방향(normal) 성분을 빼서 표면을 따라 미끄러지게 한다
		Remaining = Remaining * (1.0f - Hit.Time);
		Remaining = Remaining - Normal * Remaining.Dot(Normal);
	}
}

void UCharacterMovementComponent::SetMoveInput(const FVector& MoveInput)
{
	PendingMoveInput = MoveInput;
	PendingMoveInput.Z = 0.0f;
}

void UCharacterMovementComponent::Jump(float JumpSpeed)
{
	if (IsGrounded())
	{
		Velocity.Z = JumpSpeed;
	}
}

bool UCharacterMovementComponent::IsGrounded() const
{
	if (!CollisionComponent || !CollisionComponent->IsBlockComponent())
	{
		return false;
	}

	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	// 아래로 조금 sweep해서 걸을 수 있는 면(위를 향한 normal)에 닿으면 바닥 위에 서 있는 것
	FHitResult Hit;
	const FVector Down(0.0f, 0.0f, -FloorCheckDistance);
	if (!World->GetPhysicsScene().ComponentSweepSingle(CollisionComponent, Down, CollisionComponent->GetWorldRotation(), Hit))
	{
		return false;
	}
	return Hit.Normal.Z >= WalkableFloorZ;
}

void UCharacterMovementComponent::SetCollisionComponent(UPrimitiveComponent* InCollisionComponent)
{
	CollisionComponent = InCollisionComponent;
}

void UCharacterMovementComponent::AddKnockback(const FVector& InVelocity)
{
	KnockbackVelocity = InVelocity;
}