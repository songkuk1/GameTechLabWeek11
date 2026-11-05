#pragma once

#include "UObject/Class.h"
#include "Component/SceneComponent.h"

struct FMinimalViewInfo;

class UCameraComponent : public USceneComponent
{
	DECLARE_CLASS(UCameraComponent, USceneComponent)
	REFLECT_START(UCameraComponent)
		PROPERTY(bIsOrthogonal)
		PROPERTY(FieldOfView)
		PROPERTY(AspectRatio)
		PROPERTY(bConstrainAspectRatio)
		PROPERTY(NearClipPlane)
		PROPERTY(FarClipPlane)
		PROPERTY(OrthoWidth)
	REFLECT_END()

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
	bool bIsOrthogonal = false;
	float FieldOfView = 60.0f;
	float AspectRatio = 1.777778f; // 16:9

	// Camera Options
	bool bConstrainAspectRatio = false;

	// 그 외
	float NearClipPlane = 0.1f;
	float FarClipPlane = 10000.0f;
	float OrthoWidth = 16.0f;
};
