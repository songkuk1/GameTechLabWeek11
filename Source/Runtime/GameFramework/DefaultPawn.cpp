#include "EnginePCH.h"
#include "DefaultPawn.h"

#include "PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

ADefaultPawn::ADefaultPawn()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	CameraComponent->SetupAttachment(GetRootComponent());

	bUseControllerRotationPitch = true;
}

void ADefaultPawn::ProcessPlayerInput(float DeltaTime)
{
	Super::ProcessPlayerInput(DeltaTime);
	if (!Controller)
	{
		return;
	}

	if (FInputSystem::IsMouseDown(EMouseButton::Right))
	{
		Controller->AddYawInput(FInputSystem::GetMouseDeltaX() * MouseSensitivity);
		Controller->AddPitchInput(FInputSystem::GetMouseDeltaY() * MouseSensitivity);
	}

	const FQuat ViewQuat = Controller->GetControlRotation().Quaternion();
	const FVector Forward = ViewQuat.RotateVector(FVector{ 1.0f, 0.0f, 0.0f });
	const FVector Right = ViewQuat.RotateVector(FVector{ 0.0f, 1.0f, 0.0f });
	const FVector Up = ViewQuat.RotateVector(FVector{ 0.0f, 0.0f, 1.0f });

	FVector Direction = FVector::ZeroVector;
	if (FInputSystem::IsKeyDown(EKeyCode::W))
	{
		Direction += Forward;
	}
	if (FInputSystem::IsKeyDown(EKeyCode::S))
	{
		Direction -= Forward;
	}
	if (FInputSystem::IsKeyDown(EKeyCode::A))
	{
		Direction -= Right;
	}
	if (FInputSystem::IsKeyDown(EKeyCode::D))
	{
		Direction += Right;
	}
	if (FInputSystem::IsKeyDown(EKeyCode::Q))
	{
		Direction -= Up;
	}
	if (FInputSystem::IsKeyDown(EKeyCode::E))
	{
		Direction += Up;
	}

	FVector Delta = Direction * (MoveSpeed * DeltaTime);

	if (const int32 Wheel = FInputSystem::GetWheelDelta())
	{
		Delta += Forward * (WheelSpeed * static_cast<float>(Wheel) * DeltaTime);
	}

	if (Delta != FVector::ZeroVector)
	{
		SetActorLocation(GetActorLocation() + Delta);
	}
}