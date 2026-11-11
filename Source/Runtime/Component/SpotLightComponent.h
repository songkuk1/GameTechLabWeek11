#pragma once

#include "PointLightComponent.h"
#include "SpotLightComponent.generated.h"

class FLineBatcher;

UCLASS()
class USpotLightComponent : public UPointLightComponent
{
	GENERATED_BODY()

public:
	USpotLightComponent() = default;
	virtual ~USpotLightComponent() override = default;

	float GetInnerConeAngle() const { return InnerConeAngle; }
	float GetOuterConeAngle() const { return OuterConeAngle; }

	FSpotLightInfo GetSpotLightInfo() const;
	virtual FShadowInfo GetShadowInfo() const override;
	virtual FLightSceneProxy* CreateSceneProxy() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	// 바깥·안쪽 원뿔을 그린다.
	virtual void DrawDebug(FLineBatcher* LineBatcher) const override;
	// IsDebugLine이 켜져 있으면 선택하지 않아도 원뿔을 그린다.
	virtual bool ShouldAlwaysDrawDebug() const override { return IsDebugLine; }

protected:
	UPROPERTY(EditAnywhere)
	float InnerConeAngle = 0.0f;
	UPROPERTY(EditAnywhere)
	float OuterConeAngle = 44.0f;

private:
	UPROPERTY(EditAnywhere)
	bool IsDebugLine = false;
};
