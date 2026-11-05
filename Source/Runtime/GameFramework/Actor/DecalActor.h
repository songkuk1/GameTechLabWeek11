#pragma once

#include "GameFramework/Actor.h"
#include "Core/ResourcePaths.h"

class UDecalComponent;
class UBillboardComponent;

// 데칼 컴포넌트 및 에디터 표식 조립
class ADecalActor : public AActor
{
    DECLARE_CLASS(ADecalActor, AActor)

public:
    ADecalActor();
    ~ADecalActor() override = default;
    UDecalComponent* GetDecalComponent() const;
    UBillboardComponent* GetBillboardComponent() const;
};
