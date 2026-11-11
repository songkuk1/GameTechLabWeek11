#pragma once

#include "LightComponent.h"
#include "DirectionalLightComponent.generated.h"

class FLineBatcher;

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
	GENERATED_BODY()

public:
	UDirectionalLightComponent() { ShadowResolution = 2048; }
	virtual ~UDirectionalLightComponent() override = default;

	FDirectionalLightInfo GetDirectionalLightInfo() const;
	virtual FShadowInfo GetShadowInfo() const override;
	virtual FLightSceneProxy* CreateSceneProxy() override;

	void DrawDebug(FLineBatcher* LineBatcher) const;

	float GetOrthoWidth() const { return OrthoWidth; }
	void SetOrthoWidth(float InWidth)
	{
		OrthoWidth = InWidth;
		MarkSceneProxyDirty(); // 값이 바뀌면 프록시 갱신 요청
	}

	float GetShadowNormalBias() const { return ShadowNormalBias; }
	void SetShadowNormalBias(float InBias)
	{
		ShadowNormalBias = InBias;
		MarkSceneProxyDirty();
	}

private:
	UPROPERTY(EditAnywhere)
	float OrthoWidth = 2000.0f;
	UPROPERTY(EditAnywhere)
	float ShadowNormalBias = 2.0f;
};
