#pragma once

#include "CameraShakePattern.h"
#include "UObject/Class.h"
#include <random>

struct FWaveOscillator
{
	// 진폭 (Location: 거리, Rotation/FOV: 도)
	float Amplitude = 0.f;

	// 초당 진동 횟수(Hz)
	float Frequency = 0.f;

	bool bRandomOffset = true;

	// 시작 위상. Start 때 정한다
	float Offset = 0.f;

	void Start() 
	{ 
		Offset = bRandomOffset ? RandomRange(0, 2 * PI) : 0.f; 
	}

	float Evaluate(float Time) const
	{
		return Amplitude * std::sin(2.f * PI * Frequency * Time + Offset);
	}

	std::mt19937& GetRandomEngine()
	{
		static std::mt19937 Engine{ std::random_device{}() };
		return Engine;
	}

	float RandomRange(float Min, float Max)
	{
		return std::uniform_real_distribution(Min, Max)(GetRandomEngine());
	}
};


class UCameraShakePatternWave : public UCameraShakePattern
{
	DECLARE_CLASS(UCameraShakePatternWave, UCameraShakePattern)
	/*REFLECT_START(ClassName)
		REFLECT_END()*/
public:
	UCameraShakePatternWave();
	virtual ~UCameraShakePatternWave();

protected:
	// 7개 축 모두 Start()
	void OnStart() override;

	// ElapsedTime으로 각 축 Evaluate
	void OnUpdate(float DeltaTime, FCameraShakeUpdateResult& Out) override;  

public:
	// Location
	FWaveOscillator X, Y, Z;
	// Rotation
	FWaveOscillator Pitch, Yaw, Roll;
	FWaveOscillator FOV;

};