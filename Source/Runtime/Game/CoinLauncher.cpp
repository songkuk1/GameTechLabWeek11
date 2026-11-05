#include "EnginePCH.h"
#include "CoinLauncher.h"

#include "Component/ScriptComponent.h"
#include "Coin.h"
#include "CoinMovementComponent.h"
#include "Engine/World.h"
#include <random>

namespace
{
	constexpr float HalfAngleDeg = 15.0f;
	constexpr float SpeedJitter = 0.2f;

	std::mt19937& GetRandomEngine()
	{
		static std::mt19937 Engine{ std::random_device{}() };
		return Engine;
	}

	float RandomRange(float Min, float Max)
	{
		return std::uniform_real_distribution(Min, Max)(GetRandomEngine());
	}

	// Forward 축을 중심으로 반각 InHalfAngleDeg인 원뿔 안의 임의 방향
	FVector RandomDirectionInCone(const FVector& Forward, const FVector& Right, const FVector& Up, float InHalfAngleDeg)
	{
		const float CosTheta = 1.0f - RandomRange(0.0f, 1.0f) * (1.0f - std::cos(FMath::DegreesToRadians(InHalfAngleDeg)));
		const float SinTheta = std::sqrt(1.0f - CosTheta * CosTheta);
		const float Phi = RandomRange(0.0f, 2.0f * PI);

		return Forward * CosTheta + Right * SinTheta * std::cos(Phi) + Up * SinTheta * std::sin(Phi);
	}
}

ACoinLauncher::ACoinLauncher()
{
	ScriptComponent = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	ScriptComponent->SetScriptPath("Assets/Scripts/Actors/CoinLauncher.lua");
}

void ACoinLauncher::Launch(uint32 Amount, float Speed)
{
	LaunchCoins(World, GetActorLocation(), GetActorQuat().GetForwardVector(), HalfAngleDeg, Amount, Speed);
}

void ACoinLauncher::LaunchCoins(UWorld* InWorld, const FVector& Origin, const FVector& Axis, float InHalfAngleDeg, uint32 Amount, float Speed)
{
	if (!InWorld)
	{
		return;
	}

	// Axis를 앞 방향으로 하는 직교 기저
	const FVector Forward = Axis.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
	const FVector Helper = std::fabs(Forward.Z) < 0.99f ? FVector::UpVector : FVector::ForwardVector;
	const FVector Right = Helper.Cross(Forward).GetSafeNormal();
	const FVector Up = Forward.Cross(Right);

	const FTransform SpawnTransform(Origin, FVector(1.0), FRotator(0.f, 0.f, 0.f));

	for (uint32 i = 0; i < Amount; ++i)
	{
		ACoin* Coin = InWorld->SpawnActor<ACoin>(NAME_None, &SpawnTransform);
		if (!Coin)
		{
			continue;
		}

		const FVector Direction = RandomDirectionInCone(Forward, Right, Up, InHalfAngleDeg);
		const float JitteredSpeed = Speed * (1.0f + RandomRange(-SpeedJitter, SpeedJitter));

		UCoinMovementComponent* Movement = Coin->GetCoinMovementComponent();
		Movement->bInitialVelocityInLocalSpace = false;
		Movement->Velocity = Direction * JitteredSpeed;
		Movement->ProjectileGravityScale = 1.0f;
		Movement->bSimulationEnabled = true;
	}
}
