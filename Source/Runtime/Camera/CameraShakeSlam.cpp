#include "EnginePCH.h"
#include "CameraShakeSlam.h"

#include "Camera/CameraTypes.h"
#include "CameraShakePatternWave.h"

UCameraShakeSlam::UCameraShakeSlam()
{
	auto* Wave = NewObject<UCameraShakePatternWave>(this);
	if (Wave)
	{
		// 순간적으로 꽂히고 천천히 잦아든다
		Wave->Duration = 0.5f;
		Wave->BlendInTime = 0.0f;
		Wave->BlendOutTime = 0.35f;

		// 땅이 울리는 느낌: 위아래(Z, Pitch) 위주, 피격보다 낮은 주파수
		Wave->Z = { .Amplitude = 0.25f, .Frequency = 18.f };
		Wave->Pitch = { .Amplitude = 2.5f, .Frequency = 15.f };
		Wave->Roll = { .Amplitude = 1.0f, .Frequency = 12.f };
		Wave->Yaw = { .Amplitude = 0.5f, .Frequency = 10.f };

		RootPattern = Wave;
	}
}

UCameraShakeSlam::~UCameraShakeSlam()
{
}
