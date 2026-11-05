#include "EnginePCH.h"
#include "PlayerController.h"

#include "Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

APlayerController::APlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bInputEnabled || !Pawn)
	{
		return;
	}

	Pawn->ProcessPlayerInput(DeltaTime);
	Pawn->FaceRotation(ControlRotation, DeltaTime);
}

void APlayerController::EndPlay()
{
	UnPossess();
	Super::EndPlay();
}

void APlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SpawnPlayerCameraManager();
}

void APlayerController::Possess(APawn* InPawn)
{
	if (Pawn == InPawn)
	{
		return;
	}
	UnPossess();
	if (!InPawn)
	{
		return;
	}

	if (APlayerController* OldController = InPawn->GetController())
	{
		OldController->UnPossess();
	}

	Pawn = InPawn;
	ControlRotation = InPawn->GetActorRotation();
	Pawn->PossessedBy(this);
	SetViewTarget(Pawn);
}

void APlayerController::UnPossess()
{
	if (!Pawn)
	{
		return;
	}

	APawn* OldPawn = Pawn;
	Pawn = nullptr;

	if (PlayerCameraManager && PlayerCameraManager->GetViewTarget() == OldPawn)
	{
		//매니저가 보던 타겟이라면 PC로 전환
		ViewFromSelf();		
	}

	OldPawn->UnPossessed();
}

void APlayerController::AddYawInput(float Value)
{
	ControlRotation.Yaw += Value;
}

void APlayerController::AddPitchInput(float Value)
{
	ControlRotation.Pitch += Value;
	ControlRotation.Pitch = FMath::Clamp(ControlRotation.Pitch, -89.0f, 89.0f);
}

void APlayerController::SetViewTarget(AActor* InViewTarget)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(InViewTarget);
	}
}

AActor* APlayerController::GetViewTarget() const
{
	if (PlayerCameraManager)
	{
		return PlayerCameraManager->GetViewTarget();
	}

	return nullptr;
}

void APlayerController::UpdateCamera(float DeltaTime)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->UpdateCamera(DeltaTime);
	}
}

const FMinimalViewInfo& APlayerController::GetPlayerView() const
{	
	if (PlayerCameraManager)
	{
		return PlayerCameraManager->GetCameraCachePOV();
	}

	return FallBackView;
}

void APlayerController::SpawnPlayerCameraManager()
{
	if (World)
	{
		PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(FName("PlayerCameraManager"));
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetFlags(EObjectFlags::RF_Transient);
			PlayerCameraManager->InitializeFor(this);
		}
	}
}

void APlayerController::NotifyActorDestroyed(AActor* DestroyActor)
{
	if (!PlayerCameraManager) return;

	if (PlayerCameraManager->GetViewTarget() == DestroyActor)
	{
		//타겟이 파괴되면 PC로 되돌리기
		ViewFromSelf();

		//PendingViewTarget이 파괴되면 전환 취소하기
		//...
	}

}

void APlayerController::ViewFromSelf()
{
	if (!PlayerCameraManager) return;
	FallBackView = PlayerCameraManager->GetViewTargetPOV();
	PlayerCameraManager->SetViewTarget(this);
}

FCameraScreenEffects APlayerController::GetScreenEffects() const
{
	if (PlayerCameraManager)
	{
		return PlayerCameraManager->GetScreenEffects();
	}

	return FCameraScreenEffects();
}
