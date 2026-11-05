#include "EnginePCH.h"
#include "PreviewScene.h"

#include "Engine/Engine.h"      
#include "Engine/World.h"        
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/AmbientLightActor.h"
#include "Asset/AssetManager.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"


FPreviewScene::FPreviewScene()
{
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::EditorPreview);
    PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    Context.SetCurrentWorld(PreviewWorld);

    // 어떤 에셋을 보든 공통으로 쓰는 환경
    Floor = Cast<AStaticMeshActor>(PreviewWorld->SpawnActor(AStaticMeshActor::StaticClass()));
    // StaticMeshComponent 기본값이 Cube라 평면으로 바꾼다. Plane은 원점 중심, 한 변 1인 XY 평면이다
    UStaticMeshComponent* FloorMesh = Floor->GetStaticMeshComponent();
    FloorMesh->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Plane"));
    //바닥 사이즈 설정
    FloorMesh->SetRelativeScale3D(FVector(20.0f, 20.0f, 1.0f));
    FloorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));

    DirectionalLight = Cast<ADirectionalLightActor>(PreviewWorld->SpawnActor(ADirectionalLightActor::StaticClass()));
    // 정면 위쪽에서 비스듬히 내려 비추도록 Yaw 후 Pitch를 준다. Y축 양의 회전이 아래를 향한다
    constexpr float LightPitchRadians = 0.87f; // 약 50도
    constexpr float LightYawRadians = 0.52f;   // 약 30도
    const float SinPitch = std::sin(LightPitchRadians * 0.5f), CosPitch = std::cos(LightPitchRadians * 0.5f);
    const float SinYaw = std::sin(LightYawRadians * 0.5f), CosYaw = std::cos(LightYawRadians * 0.5f);
    const FQuat LightRotation(-SinYaw * SinPitch, CosYaw * SinPitch, CosPitch * SinYaw, CosYaw * CosPitch);
    DirectionalLight->GetLightComponent()->SetRelativeRotation(LightRotation.ToFRotator());

    AmbientLight = Cast<AAmbientLightActor>(PreviewWorld->SpawnActor(AAmbientLightActor::StaticClass()));

    // 프리뷰는 배치 편집을 하지 않으므로 라이트 위치 표시 아이콘을 숨긴다
    HideIcons(DirectionalLight);
    HideIcons(AmbientLight);

}
void FPreviewScene::HideIcons(AActor* Actor)
{
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
            Billboard->SetVisible(false);
    }
}
FPreviewScene::~FPreviewScene()
{
    if (!PreviewWorld)
        return;

    // BeginPlay를 하지 않은 월드라 EndPlay 없이 컨텍스트를 돌려준다.
    // DestroyWorldContext가 UWorld::DestroyWorld로 ClearWorld·Cleanup·delete까지 하므로 여기서 다시 지우지 않는다
    GEngine->DestroyWorldContext(PreviewWorld);
    PreviewWorld = nullptr;
}
