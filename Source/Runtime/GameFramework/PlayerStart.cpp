#include "EnginePCH.h"
#include "PlayerStart.h"

#include "Component/BillboardComponent.h"
#include "Core/ResourcePaths.h"

APlayerStart::APlayerStart()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	BillboardComponent->SetupAttachment(GetRootComponent());
	BillboardComponent->SetHiddenInDetails(false);
	BillboardComponent->SetTexture(ResourcePaths::PlayerStartIcon);
}
