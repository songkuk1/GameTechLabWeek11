#pragma once

#include "Actor.h"

class UBillboardComponent;

class APlayerStart : public AActor
{
	DECLARE_CLASS(APlayerStart, AActor)

public:
	APlayerStart();

private:
	UBillboardComponent* BillboardComponent = nullptr;
};
