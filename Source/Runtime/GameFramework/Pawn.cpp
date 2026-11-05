#include "EnginePCH.h"
#include "Pawn.h"

#include "PlayerController.h"

void APawn::EndPlay()
{
	if (Controller)
	{
		Controller->UnPossess();
	}

	Super::EndPlay();
}

void APawn::FaceRotation(const FRotator& ControlRotation, float DeltaTime)
{
	FRotator Rotation = GetActorRotation();

	if (bUseControllerRotationPitch)
	{
		Rotation.Pitch = ControlRotation.Pitch;
	}

	if (bUseControllerRotationYaw)
	{
		Rotation.Yaw = ControlRotation.Yaw;
	}

	if (bUseControllerRotationRoll)
	{
		Rotation.Roll = ControlRotation.Roll;
	}

	SetActorRotation(Rotation);
}

bool APawn::IsPlayerInputEnabled() const
{
	return Controller && Controller->IsInputEnabled();
}

void APawn::AddControllerYawInput(float Value)
{
	if (Controller)
	{
		Controller->AddYawInput(Value);
	}
}

void APawn::AddControllerPitchInput(float Value)
{
	if (Controller)
	{
		Controller->AddPitchInput(Value);
	}
}

FRotator APawn::GetControlRotation() const
{
	return Controller ? Controller->GetControlRotation() : GetActorRotation();
}
