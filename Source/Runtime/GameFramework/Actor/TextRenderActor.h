#pragma once

#include "GameFramework/Actor.h"
#include "Component/TextRenderComponent.h"
#include "TextRenderActor.generated.h"

UCLASS()
class ATextRenderActor : public AActor
{
	GENERATED_BODY()


public:
	ATextRenderActor();
	virtual ~ATextRenderActor();

	inline UTextRenderComponent* GetTextRenderComponent() const { return TextRenderComponent; }

private:
	UTextRenderComponent* TextRenderComponent;
};
