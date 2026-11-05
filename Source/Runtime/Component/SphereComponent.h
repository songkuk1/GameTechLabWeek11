#pragma once

#include "ShapeComponent.h"
#include "Collision/CollisionShape.h"

class FLineBatcher;
struct FBox;

class USphereComponent :public UShapeComponent
{
	DECLARE_CLASS(USphereComponent, UShapeComponent)

	REFLECT_START(ClassName)
		PROPERTY(SphereRadius)
	REFLECT_END()

public:
	USphereComponent();
	virtual ~USphereComponent();

	virtual void BeginPlay() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	// 월드 스케일을 곱한 반지름 반환
	float GetWorldSphereRadius()const;

	virtual FCollisionShape GetCollisionShape() const override;	

	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;

	virtual FBox CalcLocalBounds() const override;

	void SetSphereRadius(float InSphereRadius);

private:
	//로컬 반지름
	float SphereRadius = 0.5f;
};