#include "EnginePCH.h"
#include "BoxComponent.h"
#include "Render/LineBatcher.h"

UBoxComponent::UBoxComponent()
{

}

UBoxComponent::~UBoxComponent()
{

}

void UBoxComponent::BeginPlay()
{
	Super::BeginPlay();
}


//bool UBoxComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
//{
//	const FStaticMeshData* Mesh = GetMeshData();
//	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
//}

//bool UBoxComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
//{
//	const FStaticMeshData* Mesh = GetMeshData();
//	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
//}

void UBoxComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
}

FCollisionShape UBoxComponent::GetCollisionShape() const
{
	//아직 박스 데이터가 없어서 일단 이렇게
	return FCollisionShape::MakeBox(GetWorldBoxExtent());
}

void UBoxComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (!LineBatcher) return;
	//Min, Max
	const FBox LocalBox{ BoxExtent * -1.f, BoxExtent };
	LineBatcher->AddOrientedBox(LocalBox, GetWorldMatrix(), GetDebugColor());
}

FVector UBoxComponent::GetWorldBoxExtent() const
{
	const FVector S = GetWorldScale3D();
	return FVector(BoxExtent.X * std::fabs(S.X), BoxExtent.Y * std::fabs(S.Y), BoxExtent.Z * std::fabs(S.Z));
}

FBox UBoxComponent::CalcLocalBounds() const
{
	return FBox(-BoxExtent, BoxExtent);
}
