#pragma once

#include "Actor.h"
#include "Pawn.generated.h"

class APlayerController;

UCLASS()
class APawn : public AActor
{
	GENERATED_BODY()

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

	UPROPERTY(EditAnywhere)
	bool bAutoPossessPlayer = false;
	bool bUseControllerRotationPitch = false;
	bool bUseControllerRotationYaw = true;
	bool bUseControllerRotationRoll = false;
};
