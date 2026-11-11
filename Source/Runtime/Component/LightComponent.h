#pragma once

#include "LightComponentBase.h"
#include "LightComponent.generated.h"

UCLASS()
class ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()

public:
	ULightComponent() = default;
	virtual ~ULightComponent() override = default;

	FVector GetLightDirection() const
	{
		const FMatrix World = GetWorldMatrix();
		return FVector(World[0][0], World[0][1], World[0][2]).GetSafeNormal();
	}

	float GetShadowBias() const { return ShadowBias; }
	void SetShadowBias(float InShadowBias) { ShadowBias = InShadowBias; }

    // 행 벡터 규약으로 View와 Projection을 곱한다.
    FMatrix GetViewProjectionMatrix(bool bIsOrthogonal, float FarClipPlane, float FieldOfView, float AspectRatio) const;
    FMatrix GetViewProjectionMatrix(FRotator Rotation, bool bIsOrthogonal, float FarClipPlane, float FieldOfView, float AspectRatio) const;
		   
	virtual FShadowInfo GetShadowInfo() const;
	virtual void GetShadowInfo(TArray<FShadowInfo>& OutShadowInfo) const {}

	ID3D11ShaderResourceView* GetShadowPreviewSRV(int32 Index) const;
	int32 GetShadowInfoNum() const;

	//이번 프레임에서 ShadowMap을 Preview하도록한다.
	void SetPreviewShadowMapFlag(bool InShow);

protected:
    // Transform의 역변환으로 View 행렬을 만든다.
    FMatrix GetViewMatrix() const;
    FMatrix GetViewMatrix(FRotator Rotation) const;

    // 시야각·종횡비·클리핑 거리로 원근 행렬을 만든다.
    FMatrix GetPerspectiveMatrix(float FarClipPlane, float FieldOfView, float AspectRatio) const;

    // 직교 폭·종횡비·클리핑 거리로 직교 행렬을 만든다.
    FMatrix GetOrthogonalMatrix(float FarClipPlane, float AspectRatio) const;

    // 투영 모드에 대응하는 행렬을 선택한다.
    FMatrix GetProjectionMatrix(bool bIsOrthogonal, float FarClipPlane, float FieldOfView, float AspectRatio) const;


protected:
	//float FieldOfView = 60.0f;
	//float AspectRatio = 1.f;
	float NearClipPlane = 0.1f;
	//float FarClipPlane = 10000.0f;

	float OrthoWidth = 16.0f;

	UPROPERTY(EditAnywhere)
	float ShadowBias = 0.001f;

	UPROPERTY(EditAnywhere, Pow2Combo)
	uint32 ShadowResolution = 1024;	// 섀도우 기본 해상도 1024로 설정
};
