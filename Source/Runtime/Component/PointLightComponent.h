#pragma once

#include "LightComponent.h"

struct FShadowInfo;
class FLineBatcher;

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(AttenuationRadius)
		PROPERTY(LightSourceRadius)
	REFLECT_END()

public:
	UPointLightComponent() = default;
	virtual ~UPointLightComponent() override = default;

	float GetAttenuationRadius() const { return AttenuationRadius; }

	FPointLightInfo GetPointLightInfo() const;
	virtual void GetShadowInfo(TArray<FShadowInfo>& OutShadowInfo) const override;
	virtual FLightSceneProxy* CreateSceneProxy() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

	// 감쇠 반경을 세 축 평면의 원으로 그린다. Spot은 원뿔로 바꿔 그린다.
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;
	// 선택하지 않아도 디버그 라인을 그릴지. Spot의 IsDebugLine 속성이 켤 수 있다.
	virtual bool ShouldAlwaysDrawDebug() const { return false; }

protected:
	float AttenuationRadius = 10.0f;
	float LightSourceRadius = 0.03f;
};
