#pragma once

#include "Pawn.h"
#include "DefaultPawn.generated.h"

class UCameraComponent;

UCLASS()
class ADefaultPawn : public APawn
{
	GENERATED_BODY()

public:
	ADefaultPawn();
	void ProcessPlayerInput(float DeltaTime) override;
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

private:
	UCameraComponent* CameraComponent = nullptr;
	UPROPERTY(EditAnywhere)
	float MoveSpeed = 10.0f;
	UPROPERTY(EditAnywhere)
	float MouseSensitivity = 0.05f;
	UPROPERTY(EditAnywhere)
	float WheelSpeed = 0.1f;
};
