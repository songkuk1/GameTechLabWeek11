#pragma once

#include "ShapeComponent.h"
#include "Collision/CollisionShape.h"
#include "BoxComponent.generated.h"

struct FBox;

UCLASS()
class UBoxComponent :public UShapeComponent
{
	GENERATED_BODY()


public:
	UBoxComponent();
	virtual ~UBoxComponent();

	virtual void BeginPlay() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	virtual FCollisionShape GetCollisionShape() const override;

	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;

	FVector GetWorldBoxExtent()const;

	void SetBoxExtent(const FVector& InExtent) { BoxExtent = InExtent; }
	const FVector& GetBoxExtent() const { return BoxExtent; }

	virtual FBox CalcLocalBounds() const override;

private:
	//반길이
	UPROPERTY(EditAnywhere)
	FVector BoxExtent = FVector(0.5f, 0.5f, 0.5f);
};