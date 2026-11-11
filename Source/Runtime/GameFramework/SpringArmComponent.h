#pragma once

#include "Component/SceneComponent.h"
#include "SpringArmComponent.generated.h"

UCLASS()
class USpringArmComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USpringArmComponent();

	FVector GetUnfixedCameraPosition() const { return UnfixedCameraPosition; }
	bool IsCollisionFixApplied() const { return bIsCameraFixed; }

	void SetTargetArmLength(float InLength) { TargetArmLength = InLength; }
	float GetTargetArmLength() const { return TargetArmLength; }
	void SetDoCollisionTest(bool bInTest) { bDoCollisionTest = bInTest; }
	bool GetDoCollisionTest() const { return bDoCollisionTest; }

	void TickComponent(float DeltaTime) override;

private:
	// Camera
	UPROPERTY(EditAnywhere)
	float TargetArmLength = 4.0f;
	UPROPERTY(EditAnywhere)
	FVector SocketOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere)
	FVector TargetOffset = FVector::ZeroVector;

	// Camera Collision
	UPROPERTY(EditAnywhere)
	float ProbeSize = 0.15f;
	UPROPERTY(EditAnywhere)
	bool bDoCollisionTest = true;

	// Camera Settings
	UPROPERTY(EditAnywhere)
	bool bUsePawnControlRotation = true;

	// Lag
	UPROPERTY(EditAnywhere)
	bool bEnableCameraLag = false;
	UPROPERTY(EditAnywhere)
	bool bEnableCameraRotationLag = false;
	UPROPERTY(EditAnywhere)
	float CameraLagSpeed = 10.0f;
	UPROPERTY(EditAnywhere)
	float CameraRotationLagSpeed = 10.0f;
	UPROPERTY(EditAnywhere)
	float CameraLagMaxDistance = 0.0f;

	// 런타임 상태
	FVector UnfixedCameraPosition = FVector::ZeroVector;
	bool bIsCameraFixed = false;
	FVector PreviousArmOrigin = FVector::ZeroVector;
	FRotator PreviousDesiredRot = FRotator::Identity;
	bool bLagInitialized = false;
};
