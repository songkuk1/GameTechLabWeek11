#pragma once

#include "GameFramework/Actor.h"

class UScriptComponent;

class ACoinRain : public AActor
{
	DECLARE_CLASS(ACoinRain, AActor)

	REFLECT_START(ACoinRain)
		PROPERTY(Interval)
		PROPERTY(RangeX)
		PROPERTY(RangeY)
	REFLECT_END()

public:
	ACoinRain();

	void Spawn();

	float GetInterval() const { return Interval; }

private:
	UScriptComponent* ScriptComponent = nullptr;

	float Interval = 1.5f;
	float RangeX = 1.0f;
	float RangeY = 1.0f;
};
