#pragma once

#include "ShapeComponent.h"
#include "Collision/CollisionShape.h"
#include "SphereComponent.generated.h"

class FLineBatcher;
struct FBox;

UCLASS()
class USphereComponent :public UShapeComponent
{
	GENERATED_BODY()


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
	UPROPERTY(EditAnywhere)
	float SphereRadius = 0.5f;
};