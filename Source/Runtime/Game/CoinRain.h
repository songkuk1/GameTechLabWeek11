#pragma once

#include "GameFramework/Actor.h"
#include "CoinRain.generated.h"

class UScriptComponent;

UCLASS()
class ACoinRain : public AActor
{
	GENERATED_BODY()


public:
	ACoinRain();

	void Spawn();

	float GetInterval() const { return Interval; }

private:
	UScriptComponent* ScriptComponent = nullptr;

	UPROPERTY(EditAnywhere)
	float Interval = 1.5f;
	UPROPERTY(EditAnywhere)
	float RangeX = 1.0f;
	UPROPERTY(EditAnywhere)
	float RangeY = 1.0f;
};
