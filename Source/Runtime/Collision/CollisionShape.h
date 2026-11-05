#pragma once

enum class ECollisionShape : uint8 
{ 
    None, 
    Sphere,
    Box,
    Capsule,
};

struct FCollisionShape
{
    ECollisionShape ShapeType = ECollisionShape::None;
    
    float SphereRadius = 0.0f;
    FVector BoxExtent = FVector();
    float CapsuleRadius = 0.f;
    float CapsuleHalfHeight = 0.f;

    static FCollisionShape MakeSphere(float Radius);
    static FCollisionShape MakeBox(const FVector& Extent);
    static FCollisionShape MakeCapsule(float Radius, float HalfHeight);
};