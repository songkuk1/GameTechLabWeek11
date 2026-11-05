#pragma once

#include "Component/SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
	DECLARE_CLASS(USpringArmComponent, USceneComponent)
	REFLECT_START(ClassName)
		PROPERTY(TargetArmLength)
		PROPERTY(SocketOffset)
		PROPERTY(TargetOffset)
		PROPERTY(ProbeSize)
		PROPERTY(bDoCollisionTest)
		PROPERTY(bUsePawnControlRotation)
		PROPERTY(bEnableCameraLag)
		PROPERTY(bEnableCameraRotationLag)
		PROPERTY(CameraLagSpeed)
		PROPERTY(CameraRotationLagSpeed)
		PROPERTY(CameraLagMaxDistance)
	REFLECT_END()

public:
	USpringArmComponent();

	FVector GetUnfixedCameraPosition() const { return UnfixedCameraPosition; }
	bool IsCollisionFixApplied() const { return bIsCameraFixed; }

	void TickComponent(float DeltaTime) override;

private:
	// Camera
	float TargetArmLength = 4.0f;
	FVector SocketOffset = FVector::ZeroVector;
	FVector TargetOffset = FVector::ZeroVector;

	// Camera Collision
	float ProbeSize = 0.15f;
	bool bDoCollisionTest = true;

	// Camera Settings
	bool bUsePawnControlRotation = true;

	// Lag
	bool bEnableCameraLag = false;
	bool bEnableCameraRotationLag = false;
	float CameraLagSpeed = 10.0f;
	float CameraRotationLagSpeed = 10.0f;
	float CameraLagMaxDistance = 0.0f;

	// 런타임 상태
	FVector UnfixedCameraPosition = FVector::ZeroVector;
	bool bIsCameraFixed = false;
	FVector PreviousArmOrigin = FVector::ZeroVector;
	FRotator PreviousDesiredRot = FRotator::Identity;
	bool bLagInitialized = false;
};
