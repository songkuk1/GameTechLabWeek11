#pragma once

#include "Component/ProjectileMovementComponent.h"

// 코인끼리는 충돌하지 않도록 만들기 위한 전용 컴포넌트
class UCoinMovementComponent : public UProjectileMovementComponent
{
	DECLARE_CLASS(UCoinMovementComponent, UProjectileMovementComponent)

public:
	bool MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit) override;
	void TickComponent(float DeltaTime) override;
	void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	float SpinRate = 0.0f;
};
