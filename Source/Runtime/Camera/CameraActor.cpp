#include "EnginePCH.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "../Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Core/ResourcePaths.h"

// 카메라 컴포넌트를 기본 서브오브젝트로 만들어 루트에 연결한다.
ACameraActor::ACameraActor()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	SetRootComponent(CameraComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	BillboardComponent->SetupAttachment(CameraComponent);
	BillboardComponent->SetHiddenInDetails(false);
	BillboardComponent->SetTexture(ResourcePaths::CameraIcon);
}
