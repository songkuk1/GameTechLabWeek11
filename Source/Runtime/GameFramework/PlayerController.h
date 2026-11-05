#pragma once

#include "Actor.h"
#include "Camera/CameraTypes.h"

class APawn;
class APlayerCameraManager;

class APlayerController : public AActor
{
	DECLARE_CLASS(APlayerController, AActor)

public:
	APlayerController();

	void Tick(float DeltaTime) override;
	void EndPlay() override;

	virtual void PostInitializeComponents() override;
	FMinimalViewInfo CalcCamera(float DeltaTime) override { return FallBackView; }

	void Possess(APawn* InPawn);
	void UnPossess();
	APawn* GetPawn() const { return Pawn; }

	void AddYawInput(float Value);
	void AddPitchInput(float Value);
	const FRotator& GetControlRotation() const { return ControlRotation; }
	void SetControlRotation(const FRotator& InRotation) { ControlRotation = InRotation; }

	void SetViewTarget(AActor* InViewTarget);
	AActor* GetViewTarget() const;

	void UpdateCamera(float DeltaTime);
	const FMinimalViewInfo& GetPlayerView() const;

	void SetInputEnabled(bool bEnabled) { bInputEnabled = bEnabled; }
	bool IsInputEnabled() const { return bInputEnabled; }

	void SpawnPlayerCameraManager();


	void NotifyActorDestroyed(AActor* DestroyActor);

	void ViewFromSelf();

	//카메라 효과 값 가져오기
	FCameraScreenEffects GetScreenEffects()const;

	APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }

public:
	//카메라 매니저
	APlayerCameraManager* PlayerCameraManager = nullptr;


private:
	APawn* Pawn = nullptr;
	FRotator ControlRotation;
	bool bInputEnabled = true;

	FMinimalViewInfo FallBackView = {};
};
