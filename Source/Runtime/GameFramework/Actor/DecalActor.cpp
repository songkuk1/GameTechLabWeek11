#include "EnginePCH.h"
#include "GameFramework/Actor/DecalActor.h"
#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"

// 루트 및 에디터 표식 구성
ADecalActor::ADecalActor()
{
    USceneComponent* PreviousRoot = GetRootComponent();
    UDecalComponent* Decal = CreateDefaultSubobject<UDecalComponent>("DecalComponent");
    SetRootComponent(Decal);
    DestroyComponent(PreviousRoot);

    UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>("DecalBillboard");
    Billboard->SetupAttachment(Decal);
    Billboard->SetHiddenInDetails(true);
    Billboard->SetHiddenInGame(true);
    Billboard->SetFollowLightColor(false);
    Billboard->SetTexture(ResourcePaths::DecalActorIcon);
}

// 현재 소유 데칼 컴포넌트 조회
UDecalComponent* ADecalActor::GetDecalComponent() const
{
    return Cast<UDecalComponent>(FindComponentByClass(UDecalComponent::StaticClass()));
}

// 현재 소유 에디터 표식 조회
UBillboardComponent* ADecalActor::GetBillboardComponent() const
{
    return Cast<UBillboardComponent>(FindComponentByClass(UBillboardComponent::StaticClass()));
}
