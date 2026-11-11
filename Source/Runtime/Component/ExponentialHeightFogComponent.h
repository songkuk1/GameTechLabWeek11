#pragma once
#include "SceneComponent.h"
#include "Render/Foginfo.h"
#include "ExponentialHeightFogComponent.generated.h"


UCLASS()
class UExponentialHeightFogComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	UExponentialHeightFogComponent() = default;
	~UExponentialHeightFogComponent() override = default;

	UExponentialHeightFogComponent(const UExponentialHeightFogComponent&) = delete;
	UExponentialHeightFogComponent& operator=(const UExponentialHeightFogComponent&) = delete;

	FFogInfo GetFogInfo() const;

	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual void OnTransformDirty() override;

private:
	void UpdateSceneFogInfo();

	//포그의 밀도, 높이에 따른 감소율, 시작 거리, 최대 불투명도, 색상 등을 설정하는 변수들
	UPROPERTY(EditAnywhere)
	float    FogDensity = 0.02f;
	UPROPERTY(EditAnywhere)
	float    FogHeightFalloff = 0.2f;
	UPROPERTY(EditAnywhere)
	float    StartDistance = 10.0;
	UPROPERTY(EditAnywhere)
	float    FogCutoffDistance = 0.0f;
	UPROPERTY(EditAnywhere)
	float    FogMaxOpacity = 1.0f;
	// 셰이더가 그대로 쓰는 Linear 값 (언리얼 FogInscatteringColor와 같다). sRGB (1, 0.4, 0.7)을 디코딩한 값이다.
	UPROPERTY(EditAnywhere)
	FLinearColor FogColor = FLinearColor(1.0f, 0.133f, 0.448f, 1.0f);


};