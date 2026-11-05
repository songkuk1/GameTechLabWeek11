#pragma once

#include "Actor.h"

class APlayerController;

class APawn : public AActor
{
	DECLARE_CLASS(APawn, AActor)
	REFLECT_START(ClassName)
		PROPERTY(bAutoPossessPlayer)
		REFLECT_END()

public:
	void EndPlay() override;

	virtual void PossessedBy(APlayerController* NewController) { Controller = NewController; }
	virtual void UnPossessed() { Controller = nullptr; }
	
	// 빙의된 동안 PlayerController가 호출
	virtual void ProcessPlayerInput(float DeltaTime) {}

	virtual void FaceRotation(const FRotator& ControlRotation, float DeltaTime);

	APlayerController* GetController() const { return Controller; }
	bool IsPlayerInputEnabled() const;
	void AddControllerYawInput(float Value);
	void AddControllerPitchInput(float Value);

	FRotator GetControlRotation() const;
	bool IsAutoPossessPlayer() const { return bAutoPossessPlayer; }

protected:
	APlayerController* Controller = nullptr;

	bool bAutoPossessPlayer = false;
	bool bUseControllerRotationPitch = false;
	bool bUseControllerRotationYaw = true;
	bool bUseControllerRotationRoll = false;
};
