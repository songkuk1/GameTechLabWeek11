#pragma once
#include "MovementComponent.h"
#include "SceneComponent.h"

class UPrimitiveComponent;

class UCharacterMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(UCharacterMovementComponent, UMovementComponent)
	REFLECT_START(ClassName)
		PROPERTY(RotationRate)
		PROPERTY(bOrientRotationToMovement)
	REFLECT_END()

public:
	UCharacterMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	void SetMoveInput(const FVector& MoveInput);
	void Jump(float JumpSpeed);
	// 콜리전 바로 아래에 걸을 수 있는 바닥이 있는가 (아래로 짧게 sweep해서 확인)
	bool IsGrounded() const;

	void SetCollisionComponent(UPrimitiveComponent* InCollisionComponent);

	void AddKnockback(const FVector& InVelocity);

private:
	// Delta만큼 sweep 이동하고, 막히면 남은 이동을 표면을 따라 미끄러뜨린다
	void MoveWithSlide(const FVector& Delta, const FRotator& NewRotation);

	FVector PendingMoveInput = FVector::ZeroVector;

	float MoveSpeed = 5.0f;

	// Hit normal의 Z가 이 값 이상이면 걸을 수 있는 바닥 (약 45도 경사까지)
	float WalkableFloorZ = 0.7f;
	// 바닥 확인용으로 아래로 sweep하는 거리. 이동이 남기는 PullBack(0.01)보다 커야 한다
	float FloorCheckDistance = 0.05f;
	// 한 프레임 이동에서 미끄러짐을 반복하는 최대 횟수 (바닥 + 벽 모서리 등)
	int32 MaxSlideIterations = 3;

	float RotationRate = 540.0f;
	bool bOrientRotationToMovement = true;

	UPrimitiveComponent* CollisionComponent = nullptr;

	FVector KnockbackVelocity = FVector::ZeroVector;
	float KnockbackDeceleration = 10.0f;
};
