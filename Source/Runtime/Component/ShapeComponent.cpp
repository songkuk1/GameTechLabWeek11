#include "EnginePCH.h"
#include "ShapeComponent.h"
#include "Render/LineBatcher.h"

UShapeComponent::UShapeComponent()
{
	bCastShadow = false;
	bReceiveShadow = false;
}

UShapeComponent::~UShapeComponent()
{

}

void UShapeComponent::BeginPlay()
{
	Super::BeginPlay();
}


//bool UShapeComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
//{
//	const FStaticMeshData* Mesh = GetMeshData();
//	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
//}

//bool UShapeComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
//{
//	const FStaticMeshData* Mesh = GetMeshData();
//	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
//}

void UShapeComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
}

FVector4 UShapeComponent::GetDebugColor() const
{
	// 디버그 라인은 오버레이라 화면에 보이는 sRGB 값을 감마 변환 없이 그대로 쓴다
	FLinearColor LinearColor = ShapeColor.ReinterpretAsLinear();
	return FVector4(LinearColor.R, LinearColor.G, LinearColor.B, 1.f);
}

void UShapeComponent::DrawArc(FLineBatcher* LineBatcher, const FVector& Center, const FVector& AxisX, const FVector& AxisY, float Radius, float StartAngle, float EndAngle, int32 Segments, const FVector4& Color)
{
	// 선분 시작 위치
	FVector Prev = Center + AxisX * cos(StartAngle) * Radius + AxisY * sin(StartAngle) * Radius;
	for (int i = 1; i <= Segments; ++i)
	{
		//그리고자 하는 각도(EndAngle - StartAngle)을 1 / Segments 로 나눠 그린다.
		float a = StartAngle + (EndAngle - StartAngle) * i / Segments;
		//a 각도로부터 선분 끝 구하기
		FVector P = Center + AxisX * cos(a) * Radius + AxisY * sin(a) * Radius;
		LineBatcher->AddLine(Prev, P, Color);
		Prev = P;
	}
}

