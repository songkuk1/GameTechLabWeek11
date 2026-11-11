#pragma once

#include "PrimitiveComponent.h"
#include "ShapeComponent.generated.h"

class FLineBatcher;

UCLASS()
class UShapeComponent :public UPrimitiveComponent
{
	GENERATED_BODY()


public:
	UShapeComponent();
	virtual ~UShapeComponent();

	virtual void BeginPlay() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	virtual void DrawDebug(FLineBatcher* LineBatcher) const {}

	bool ShouldAlwaysDrawDebug() const { return !bDrawOnlyIfSelected; }

	bool GetIsHiddenLine() { return bIsGameHidden; }

protected:
	FVector4 GetDebugColor() const;   

	// Center에서 AxisX·AxisY 평면 위, StartAngle~EndAngle 구간의 호를 그린다 (2π면 원)
	static void DrawArc(FLineBatcher* LineBatcher, const FVector& Center,
						const FVector& AxisX, const FVector& AxisY, float Radius,
						float StartAngle, float EndAngle, int32 Segments, const FVector4& Color);

protected:
	UPROPERTY(EditAnywhere)
	FColor ShapeColor = FColor();
	UPROPERTY(EditAnywhere)
	bool bDrawOnlyIfSelected = false;
	UPROPERTY(EditAnywhere)
	bool bIsGameHidden = true;
private:
};