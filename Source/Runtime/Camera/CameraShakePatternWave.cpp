#include "EnginePCH.h"
#include "CameraShakePatternWave.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"

UCameraShakePatternWave::UCameraShakePatternWave()
{
}

UCameraShakePatternWave::~UCameraShakePatternWave()
{
}

void UCameraShakePatternWave::OnStart()
{
	X.Start(); 
	Y.Start(); 
	Z.Start();
	Pitch.Start(); 
	Yaw.Start(); 
	Roll.Start();
	FOV.Start();
}

void UCameraShakePatternWave::OnUpdate(float DeltaTime, FCameraShakeUpdateResult& Out)
{
	Super::OnUpdate(DeltaTime, Out);

	const float T = ElapsedTime;   // 부모가 이미 DeltaTime을 더해 둔 값
	Out.Location = FVector(X.Evaluate(T), Y.Evaluate(T), Z.Evaluate(T));
	Out.Rotation.Pitch = Pitch.Evaluate(T);
	Out.Rotation.Yaw = Yaw.Evaluate(T);
	Out.Rotation.Roll = Roll.Evaluate(T);
	Out.FOV = FOV.Evaluate(T);
}
