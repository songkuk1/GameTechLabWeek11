#include "EnginePCH.h"
#include "CollisionShape.h"

FCollisionShape FCollisionShape::MakeSphere(float Radius)
{
	return FCollisionShape(ECollisionShape::Sphere, Radius, FVector(), 0.f, 0.f);    
}

FCollisionShape FCollisionShape::MakeBox(const FVector& Extent)
{
	return FCollisionShape(ECollisionShape::Box, 0.f, Extent, 0.f, 0.f);
}

FCollisionShape FCollisionShape::MakeCapsule(float Radius, float HalfHeight)
{
	return FCollisionShape(ECollisionShape::Capsule, 0.f, FVector(), Radius, HalfHeight);
}
