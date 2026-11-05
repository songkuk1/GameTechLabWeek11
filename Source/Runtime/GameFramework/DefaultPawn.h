#pragma once

#include "Pawn.h"

class UCameraComponent;

class ADefaultPawn : public APawn
{
	DECLARE_CLASS(ADefaultPawn, APawn)
	REFLECT_START(ClassName)
		PROPERTY(MoveSpeed)
		PROPERTY(MouseSensitivity)
		PROPERTY(WheelSpeed)
		REFLECT_END()

public:
	ADefaultPawn();
	void ProcessPlayerInput(float DeltaTime) override;
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

private:
	UCameraComponent* CameraComponent = nullptr;
	float MoveSpeed = 10.0f;
	float MouseSensitivity = 0.05f;
	float WheelSpeed = 0.1f;
};
