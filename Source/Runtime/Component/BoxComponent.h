#pragma once

#include "ShapeComponent.h"
#include "Collision/CollisionShape.h"

struct FBox;

class UBoxComponent :public UShapeComponent
{
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

	REFLECT_START(ClassName)
		PROPERTY(BoxExtent)
	REFLECT_END()

public:
	UBoxComponent();
	virtual ~UBoxComponent();

	virtual void BeginPlay() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	virtual FCollisionShape GetCollisionShape() const override;

	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;

	FVector GetWorldBoxExtent()const;

	virtual FBox CalcLocalBounds() const override;

private:
	//반길이
	FVector BoxExtent = FVector(0.5f, 0.5f, 0.5f);
};