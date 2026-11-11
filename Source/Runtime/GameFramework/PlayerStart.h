#pragma once

#include "Actor.h"
#include "PlayerStart.generated.h"

class UBillboardComponent;

UCLASS()
class APlayerStart : public AActor
{
	GENERATED_BODY()

public:
	APlayerStart();

private:
	UBillboardComponent* BillboardComponent = nullptr;
};
