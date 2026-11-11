#pragma once

#include "GameFramework/Actor.h"
#include "CoinLauncher.generated.h"

class UScriptComponent;

UCLASS()
class ACoinLauncher : public AActor
{
	GENERATED_BODY()

public:
	ACoinLauncher();

	void Launch(uint32 Amount, float Speed);

	// Origin에서 Axis 방향 원뿔(반각 HalfAngleDeg) 안으로 코인을 흩뿌린다. 런처 액터 없이도 쓸 수 있다.
	static void LaunchCoins(UWorld* InWorld, const FVector& Origin, const FVector& Axis, float HalfAngleDeg, uint32 Amount, float Speed);

private:
	UScriptComponent* ScriptComponent = nullptr;
};
