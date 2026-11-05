#include "EnginePCH.h"
#include "SphereComponent.h"
#include "Render/LineBatcher.h"
#include "Collision/PhysicsInterface.h"

USphereComponent::USphereComponent()
{

}

USphereComponent::~USphereComponent()
{

}

void USphereComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USphereComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
}

float USphereComponent::GetWorldSphereRadius() const
{
	//스케일 절댓값 중 가장 작은 거
	float ScaleX = std::fabs(GetWorldScale3D().X);
	float ScaleY = std::fabs(GetWorldScale3D().Y);
	float ScaleZ = std::fabs(GetWorldScale3D().Z);

	float Min = ScaleX < ScaleY ? ScaleX : ScaleY;
	Min = Min < ScaleZ ? Min : ScaleZ;

	return SphereRadius * Min;
}

FCollisionShape USphereComponent::GetCollisionShape() const
{
	return FCollisionShape::MakeSphere(GetWorldSphereRadius());
}

void USphereComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (!LineBatcher) return;

	constexpr int32 CircleSegmentCount = 32;
	const FVector Center = GetWorldLocation();

	// XY·XZ·YZ 평면의 원 세 개로 감쇠 반경 구를 나타낸다.
	const FVector PlaneAxes[3][2] =
	{
		{ FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f) },
		{ FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) },
		{ FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) },
	};

	float Radius = GetWorldSphereRadius();

	for (const auto& Axes : PlaneAxes)
	{
		//2 * PI로 원그리기
		DrawArc(LineBatcher, Center, Axes[0], Axes[1], Radius, 0.f, 2.f * PI, CircleSegmentCount, GetDebugColor());
	}
}

FBox USphereComponent::CalcLocalBounds() const
{	
	FVector R = FVector(SphereRadius, SphereRadius, SphereRadius);
	return FBox(-R, R);
}

void USphereComponent::SetSphereRadius(float InSphereRadius)
{
	SphereRadius = std::max(0.0f, InSphereRadius);
	MarkTransformDirty();
}
