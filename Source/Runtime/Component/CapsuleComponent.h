#pragma once

#include "ShapeComponent.h"
#include "Collision/CollisionShape.h"
#include "CapsuleComponent.generated.h"

struct FBox;

UCLASS()
class UCapsuleComponent :public UShapeComponent
{
	GENERATED_BODY()


public:
	UCapsuleComponent();
	virtual ~UCapsuleComponent();

	virtual void BeginPlay() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	virtual FCollisionShape GetCollisionShape() const override;

	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;

	float GetWorldCapsuleRadius()const;

	float GetWorldCapsuleHalfHeight()const;

	virtual FBox CalcLocalBounds() const override;		

private:
	//반구를 포함한 절반 높이
	UPROPERTY(EditAnywhere)
	float CapsuleHalfHeight = 1.f;
	//반구의 반지름
	UPROPERTY(EditAnywhere)
	float CapsuleRadius = 0.5f;
};