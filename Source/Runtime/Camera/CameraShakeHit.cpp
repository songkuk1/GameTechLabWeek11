#include "EnginePCH.h"
#include "CameraShakeHit.h"

#include "Camera/CameraTypes.h"
#include "CameraShakePatternWave.h"

UCameraShakeHit::UCameraShakeHit()
{
	auto* Wave = NewObject<UCameraShakePatternWave>(this);
	if (Wave)
	{
		Wave->Duration = 0.25f; 
		Wave->BlendInTime = 0.02f; 
		Wave->BlendOutTime = 0.15f;

		Wave->Pitch = { .Amplitude = 1.5f, .Frequency = 25.f };
		Wave->Yaw = { .Amplitude = 1.0f, .Frequency = 20.f };
		Wave->Z = { .Amplitude = 0.05f, .Frequency = 30.f };

		RootPattern = Wave;
	}
	
}

UCameraShakeHit::~UCameraShakeHit()
{
}
