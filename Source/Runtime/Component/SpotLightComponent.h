#pragma once

#include "PointLightComponent.h"

class FLineBatcher;

class USpotLightComponent : public UPointLightComponent
{
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)
	REFLECT_START(USpotLightComponent)
		PROPERTY(InnerConeAngle)
		PROPERTY(OuterConeAngle)
		PROPERTY(IsDebugLine)
	REFLECT_END()

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
	float InnerConeAngle = 0.0f;
	float OuterConeAngle = 44.0f;

private:
	bool IsDebugLine = false;
};
