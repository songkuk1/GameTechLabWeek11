#pragma once

#include "ShapeComponent.h"
#include "Collision/CollisionShape.h"

struct FBox;

class UCapsuleComponent :public UShapeComponent
{
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

	REFLECT_START(ClassName)
		PROPERTY(CapsuleHalfHeight)
		PROPERTY(CapsuleRadius)
	REFLECT_END()

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
	float CapsuleHalfHeight = 1.f;
	//반구의 반지름
	float CapsuleRadius = 0.5f;
};