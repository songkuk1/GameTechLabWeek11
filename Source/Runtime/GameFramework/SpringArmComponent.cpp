#include "EnginePCH.h"
#include "SpringArmComponent.h"

#include "Pawn.h"
#include "Engine/World.h"

USpringArmComponent::USpringArmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::PostUpdateWork;
}

void USpringArmComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (bUsePawnControlRotation)
	{
		if (const APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			FRotator DesiredRot = Pawn->GetControlRotation();
			if (bEnableCameraRotationLag && bLagInitialized)
			{
				DesiredRot = FMath::QExpSmoothTo(PreviousDesiredRot.Quaternion(), DesiredRot.Quaternion(), DeltaTime, CameraRotationLagSpeed).ToFRotator();
			}

			PreviousDesiredRot = DesiredRot;
			SetWorldRotation(DesiredRot, false);
		}
	}

	const FVector ArmOrigin = GetWorldLocation() + TargetOffset;
	FVector LaggedOrigin = ArmOrigin;
	if (bEnableCameraLag && bLagInitialized)
	{
		LaggedOrigin = FMath::VExpSmoothTo(PreviousArmOrigin, ArmOrigin, DeltaTime, CameraLagSpeed);

		const FVector FromOrigin = LaggedOrigin - ArmOrigin;
		if (CameraLagMaxDistance > 0.0f && FromOrigin.SizeSquared() > CameraLagMaxDistance * CameraLagMaxDistance)
		{
			LaggedOrigin = ArmOrigin + FromOrigin.GetSafeNormal() * CameraLagMaxDistance;
		}
	}
	PreviousArmOrigin = LaggedOrigin;
	bLagInitialized = true;

	const FVector DesiredLocation = LaggedOrigin + GetWorldRotation().Quaternion().RotateVector(FVector{ -TargetArmLength, 0.0f, 0.0f } + SocketOffset);
	UnfixedCameraPosition = DesiredLocation;

	FVector ResultLocation = DesiredLocation;
	bIsCameraFixed = false;

	const FVector ArmDelta = DesiredLocation - ArmOrigin;
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (bDoCollisionTest && World && ArmDelta.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		FHitResult Hit;
		if (World->GetPhysicsScene().SweepSingleByShape(*World, FCollisionShape::MakeSphere(ProbeSize), ArmOrigin, ArmDelta, FRotator(), Owner, Hit))
		{
			ResultLocation = Hit.Location;
			bIsCameraFixed = true;
		}
	}

	for (USceneComponent* Child : GetAttachChildren())
	{
		Child->SetWorldLocation(ResultLocation, false);
	}
}
