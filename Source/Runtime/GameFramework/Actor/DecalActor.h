#pragma once

#include "GameFramework/Actor.h"
#include "Core/ResourcePaths.h"
#include "DecalActor.generated.h"

class UDecalComponent;
class UBillboardComponent;

// 데칼 컴포넌트 및 에디터 표식 조립
UCLASS()
class ADecalActor : public AActor
{
    GENERATED_BODY()

public:
    ADecalActor();
    ~ADecalActor() override = default;
    UDecalComponent* GetDecalComponent() const;
    UBillboardComponent* GetBillboardComponent() const;
};
