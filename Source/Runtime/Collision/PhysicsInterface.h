#pragma once

#include "Math/EngineMath.h"
#include "Math/Box.h"
#include "CollisionShape.h"
#include "HitResult.h"

class FPhysicsInterface
{
public:
    // 두 형상이 겹치는가. 형상과 위치·회전을 따로 받는다.
    static bool OverlapTest(const FCollisionShape& ShapeA, const FVector& PosA, const FRotator& RotA,
                            const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB);
       
    //A를 Start에서 Delta만큼 갈 때 B에 처음 닿는가
    static bool SweepTest(const FCollisionShape& ShapeA, const FVector& Start, const FVector& Delta, const FRotator& RotA,
                          const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB,
                          FHitResult& OutHit);

    //범용 Sweep
    static bool SweepByBisection(const FCollisionShape& ShapeA, const FVector& Start, const FVector& Delta, const FRotator& RotA,
                                 const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB,
                                 FHitResult& OutHit);

    // 형상을 감싸는 월드 AABB. 정밀 판정 전에 멀리 떨어진 쌍을 거르는 용도
    static FBox CalcShapeBounds(const FCollisionShape& Shape, const FVector& Pos, const FRotator& Rot);

private:
    static bool SphereSphere(const FVector& CenterA, float RadiusA,
                             const FVector& CenterB, float RadiusB);

    static bool SweepSphereSphere(const FVector& Start, const FVector& Delta, float RadiusA,
                                  const FVector& CenterB, float RadiusB, FHitResult& OutHit);

    static bool SphereBox(const FVector& CenterSphere, float RadiusSphere,
                          const FVector& CenterBox, const FVector& BoxExtent, const FRotator& BoxRot);

    static bool SphereCapsule(const FVector& CenterSphere, float RadiusSphere,
                          const FVector& CenterCapsule, float RadiusCapsule, float HalfHeightCapsule, const FRotator& CapsuleRot);
    
    static bool CapsuleCapsule(const FVector& CenterA, const FRotator& RotA, float HalfHeightA, float RadiusA,
                               const FVector& CenterB, const FRotator& RotB, float HalfHeightB, float RadiusB);

    static bool BoxBox(const FVector& CenterA, const FVector& ExtentA, const FRotator& RotA,
                       const FVector& CenterB, const FVector& ExtentB, const FRotator& RotB);

    //Helper 함수/ 
    // 캡슐의 선분 구하기
    static void GetCapsuleSegment(const FVector& Center, const FRotator& Rot,
                                  float HalfHeight, float Radius,
                                  FVector& OutP0, FVector& OutP1);

    // 선분 P0–P1 위에서 Point와 가장 가까운 점
    static FVector ClosestPointOnSegment(const FVector& Point, const FVector& P0, const FVector& P1);

    //두 선분 사이 가장 가까운 위치 쌍을 구하고, 그 거리의 제곱을 반환
    static float ClosesPointsBetweenSegments(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2, FVector& OutC1, FVector& OutC2);

    //회전 축 반환
    static void GetBoxAxes(const FRotator& Rot, FVector OutAxes[3]);

    // L 축에서 두 박스의 투영이 떨어져 있으면 true
    static bool IsSeparatedOnAxis(const FVector& L, const FVector& T,
                                  const FVector AxesA[3], const FVector& ExtentA,
                                  const FVector AxesB[3], const FVector& ExtentB);

    // 겹쳐 있다면 B -> A 방향 normal과 침투 깊이를 구한다. 안 겹치면 false
    static bool ComputePenetration(const FCollisionShape& ShapeA, const FVector& PosA, const FRotator& RotA,
                                   const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB,
                                   FVector& OutNormal, float& OutDepth);

    static float GetThinScale(const FCollisionShape& Shape);

    // lo에서 멈추고, normal은 살짝 겹친 hi에서 구한다
    static void FillHitResult(const FCollisionShape& ShapeA, const FVector& Start, const FVector& Delta, const FRotator& RotA,
                              const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB,
                              float lo, float hi, FHitResult& OutHit);

    // PA: A 뼈대 위 최근접점, PB: B 뼈대 위 최근접점, RadiusSum: 두께 합
    static bool PenFromClosestPoints(const FVector& PA, const FVector& PB, float RadiusSum,
                              FVector& OutNormal, float& OutDepth);

    // 구(A) – 박스(B). normal은 박스 -> 구
    static bool PenSphereBox(const FVector& CenterSphere, float Radius,
                             const FVector& CenterBox, const FVector& Extent, const FRotator& BoxRot,
                             FVector& OutNormal, float& OutDepth);

    // L 축에서 두 박스 그림자가 겹친 길이. 0 이하면 틈
    static float ProjectedOverlap(const FVector& L, const FVector& T,
                                  const FVector AxesA[3], const FVector& ExtentA,
                                  const FVector AxesB[3], const FVector& ExtentB);

    static bool PenBoxBox(const FVector& CenterA, const FVector& ExtentA, const FRotator& RotA,
                          const FVector& CenterB, const FVector& ExtentB, const FRotator& RotB,
                          FVector& OutNormal, float& OutDepth);

    // 캡슐(A) – 박스(B) 겹침
    static bool CapsuleBox(const FVector& CenterCapsule, const FRotator& CapsuleRot, float HalfHeight, float Radius,
                           const FVector& CenterBox, const FVector& BoxExtent, const FRotator& BoxRot);

    // 캡슐(A) – 박스(B). normal은 박스 -> 캡슐
    static bool PenCapsuleBox(const FVector& CenterCapsule, const FRotator& CapsuleRot, float HalfHeight, float Radius,
                              const FVector& CenterBox, const FVector& BoxExtent, const FRotator& BoxRot,
                              FVector& OutNormal, float& OutDepth);

    // 박스 위에서 Point와 가장 가까운 점 (박스 안이면 Point 자신)
    static FVector ClosestPointOnBox(const FVector& Point, const FVector& BoxCenter, const FVector BoxAxes[3], const FVector& BoxExtent);

    // 선분 P0–P1과 박스 사이 최근접점 쌍을 구하고 거리의 제곱을 반환한다
    static float ClosestPointsSegmentBox(const FVector& P0, const FVector& P1,
                                         const FVector& BoxCenter, const FVector BoxAxes[3], const FVector& BoxExtent,
                                         FVector& OutOnSegment, FVector& OutOnBox);


private:
    static constexpr float Epsilon = 1.0e-6f;
};