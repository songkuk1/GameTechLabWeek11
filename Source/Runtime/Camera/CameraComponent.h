#pragma once

#include "UObject/Class.h"
#include "Component/SceneComponent.h"
#include "CameraComponent.generated.h"

struct FMinimalViewInfo;

UCLASS()
class UCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	FMinimalViewInfo GetCameraView() const;

	// Get & Set
	float GetFieldOfView() const { return FieldOfView; }
	void SetFieldOfView(float InFieldOfView) { FieldOfView = InFieldOfView; }

	float GetNearZ() const { return NearClipPlane; }
	void SetNearZ(float InNearZ) { NearClipPlane = InNearZ; }

	float GetFarZ() const { return FarClipPlane; }
	void SetFarZ(float InFarZ) { FarClipPlane = InFarZ; }

	bool GetIsOrthogonal() const { return bIsOrthogonal; }
	void SetIsOrthogonal(bool bInIsOrthographic) { bIsOrthogonal = bInIsOrthographic; }

	float GetOrthoWidth() const { return OrthoWidth; }
	void SetOrthoWidth(float InOrthoWidth) { OrthoWidth = InOrthoWidth; }

	float GetAspectRatio() const { return AspectRatio; }
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }

	bool GetConstrainAspectRatio() const { return bConstrainAspectRatio; }
	void SetConstrainAspectRatio(bool bInConstrainAspectRatio) { bConstrainAspectRatio = bInConstrainAspectRatio; }

protected:
	// Camera Settings
	UPROPERTY(EditAnywhere)
	bool bIsOrthogonal = false;
	UPROPERTY(EditAnywhere)
	float FieldOfView = 60.0f;
	UPROPERTY(EditAnywhere)
	float AspectRatio = 1.777778f; // 16:9

	// Camera Options
	UPROPERTY(EditAnywhere)
	bool bConstrainAspectRatio = false;

	// 그 외
	UPROPERTY(EditAnywhere)
	float NearClipPlane = 0.1f;
	UPROPERTY(EditAnywhere)
	float FarClipPlane = 10000.0f;
	UPROPERTY(EditAnywhere)
	float OrthoWidth = 16.0f;
};
