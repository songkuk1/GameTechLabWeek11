#include "EnginePCH.h"
#include "CoinRain.h"

#include "Coin.h"
#include "Component/ScriptComponent.h"
#include "Engine/World.h"
#include <random>

namespace
{
	std::mt19937& GetRandomEngine()
	{
		static std::mt19937 Engine{ std::random_device{}() };
		return Engine;
	}

	float RandomRange(float Min, float Max)
	{
		return std::uniform_real_distribution(Min, Max)(GetRandomEngine());
	}
}

ACoinRain::ACoinRain()
{
	ScriptComponent = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	ScriptComponent->SetScriptPath("Assets/Scripts/Actors/CoinRain.lua");
}

void ACoinRain::Spawn()
{
	const FVector Location = GetActorLocation() + FVector{ RandomRange(-RangeX * 0.5f, RangeX * 0.5f), RandomRange(-RangeY * 0.5f, RangeY * 0.5f), 0.0f };
	const FTransform SpawnTransform(Location, FVector(1.0), FRotator(0.f, 0.f, 0.f));

	ACoin* Coin = World->SpawnActor<ACoin>(NAME_None, &SpawnTransform);
}
