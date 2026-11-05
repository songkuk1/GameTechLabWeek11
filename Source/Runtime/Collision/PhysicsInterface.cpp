#include "EnginePCH.h"
#include "PhysicsInterface.h"
#include <algorithm>

bool FPhysicsInterface::OverlapTest(const FCollisionShape& ShapeA, const FVector& PosA, const FRotator& RotA, 
									const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB)
{
	//Sphere vs Sphere
	if (ShapeA.ShapeType == ECollisionShape::Sphere && ShapeB.ShapeType == ECollisionShape::Sphere)
	{
		return SphereSphere(PosA, ShapeA.SphereRadius, PosB, ShapeB.SphereRadius);
	}
	//Sphere vs Box
	else if (ShapeA.ShapeType == ECollisionShape::Sphere && ShapeB.ShapeType == ECollisionShape::Box)
	{
		return SphereBox(PosA, ShapeA.SphereRadius, PosB, ShapeB.BoxExtent, RotB);
	}
	//Box vs Sphere
	else if (ShapeA.ShapeType == ECollisionShape::Box && ShapeB.ShapeType == ECollisionShape::Sphere)
	{
		return SphereBox(PosB, ShapeB.SphereRadius, PosA, ShapeA.BoxExtent, RotA);
	}
	//Sphere vs Capsule
	else if (ShapeA.ShapeType == ECollisionShape::Sphere && ShapeB.ShapeType == ECollisionShape::Capsule)
	{
		return SphereCapsule(PosA, ShapeA.SphereRadius, PosB, ShapeB.CapsuleRadius, ShapeB.CapsuleHalfHeight, RotB);
	}
	//Capsule vs Sphere
	else if (ShapeA.ShapeType == ECollisionShape::Capsule && ShapeB.ShapeType == ECollisionShape::Sphere)
	{
		return SphereCapsule(PosB, ShapeB.SphereRadius, PosA, ShapeA.CapsuleRadius, ShapeA.CapsuleHalfHeight, RotA);
	}	
	//Capsule vs Capsule
	else if (ShapeA.ShapeType == ECollisionShape::Capsule && ShapeB.ShapeType == ECollisionShape::Capsule)
	{
		return CapsuleCapsule(PosA, RotA, ShapeA.CapsuleHalfHeight, ShapeA.CapsuleRadius, PosB, RotB, ShapeB.CapsuleHalfHeight, ShapeB.CapsuleRadius);
	}
	//Box vs Box
	else if (ShapeA.ShapeType == ECollisionShape::Box && ShapeB.ShapeType == ECollisionShape::Box)
	{
		return BoxBox(PosA, ShapeA.BoxExtent, RotA, PosB, ShapeB.BoxExtent, RotB);
	}
	//Capsule vs Box
	else if (ShapeA.ShapeType == ECollisionShape::Capsule && ShapeB.ShapeType == ECollisionShape::Box)
	{
		return CapsuleBox(PosA, RotA, ShapeA.CapsuleHalfHeight, ShapeA.CapsuleRadius, PosB, ShapeB.BoxExtent, RotB);
	}
	//Box vs Capsule
	else if (ShapeA.ShapeType == ECollisionShape::Box && ShapeB.ShapeType == ECollisionShape::Capsule)
	{
		return CapsuleBox(PosB, RotB, ShapeB.CapsuleHalfHeight, ShapeB.CapsuleRadius, PosA, ShapeA.BoxExtent, RotA);
	}
	return false;
}

bool FPhysicsInterface::SweepTest(const FCollisionShape& ShapeA, const FVector& Start, const FVector& Delta, const FRotator& RotA, 
								  const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB, FHitResult& OutHit)
{
	//Sphere vs Sphere
	if (ShapeA.ShapeType == ECollisionShape::Sphere && ShapeB.ShapeType == ECollisionShape::Sphere)
	{
		return SweepSphereSphere(Start, Delta, ShapeA.SphereRadius, PosB, ShapeB.SphereRadius, OutHit);
	}
	return SweepByBisection(ShapeA, Start, Delta, RotA, ShapeB, PosB, RotB, OutHit);
}

bool FPhysicsInterface::SweepByBisection(const FCollisionShape& ShapeA, const FVector& Start, const FVector& Delta, const FRotator& RotA, const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB, FHitResult& OutHit)
{
	float Dist = Delta.Size();
	if (Dist < Epsilon)
	{
		return false;
	}

	OutHit = FHitResult();

	FVector Normal;
	float Depth;
	//이미 겹쳐있는지
	if (ComputePenetration(ShapeA, Start, RotA, ShapeB, PosB, RotB, Normal, Depth))
	{
		//파고드는 방향
		if (Normal.Dot(Delta) < 0)
		{
			OutHit.Time = 0.f;
			OutHit.bStartPentrating = true;
			OutHit.Normal = Normal;
			OutHit.PenetrationDepth = Depth;
			OutHit.bBlockingHit = true;
			OutHit.Location = Start;
			OutHit.ImpactNormal = Normal;
			OutHit.TraceStart = Start;
			OutHit.TraceEnd = Start + Delta;
			return true;
		}
		//빠져나가는 방향 : 이동 허용
		else
		{
			return false;
		}		
	}
	float MaxStep = 32.f;
	float StepA = GetThinScale(ShapeA);
	float StepB = GetThinScale(ShapeB);
		
	float Step = 0.5f * std::min(StepA, StepB);

	//혹시 형상이 none일 때 방어
	if (Step < Epsilon)
	{
		Step = Dist;
	}

	int32 N = static_cast<int32>(FMath::Clamp(ceil(Dist / Step), 1.f, MaxStep));
	float tFree = 0;

	for (int k = 1; k <= N; ++k)
	{
		float tk = static_cast<float>(k) / static_cast<float>(N);
		if (OverlapTest(ShapeA, Start + Delta * tk, RotA, ShapeB, PosB, RotB))
		{
			// 안 겹침
			float lo = tFree;
			// 겹침
			float hi = tk;

			// 구간을 12번 반으로 줄인다 (오차 = 한 칸 / 2^12 = 1/4096)
			for (int32 Iter = 0; Iter < 12; ++Iter)
			{
				const float mid = (lo + hi) * 0.5f;

				if (OverlapTest(ShapeA, Start + Delta * mid, RotA, ShapeB, PosB, RotB))
				{
					// mid에서 겹침 -> 경계는 더 앞
					hi = mid;
				}
				else
				{
					// mid에서 안 겹침 -> 경계는 더 뒤
					lo = mid;
				}
			}

			// lo = 닿기 직전 (여기 멈춘다),  hi = 닿은 직후 (normal은 여기서 구한다)
			FillHitResult(ShapeA, Start, Delta, RotA, ShapeB, PosB, RotB, lo, hi, OutHit);
			return true;
		}
		tFree = tk;
	}
	//끝까지 안 겹침
	return false;
}

bool FPhysicsInterface::SphereSphere(const FVector& CenterA, float RadiusA, const FVector& CenterB, float RadiusB)
{
	//d = cB - cA
	// d * d <= (rA + Rb)^2

	//HTR_LOG(Warning, "d : {}, r : {}", (CenterB - CenterA).SizeSquared(), std::pow(RadiusA + RadiusB, 2));
	//딱 붙을 때는 Overlap은 아님
	return (CenterB - CenterA).SizeSquared() < std::pow(RadiusA + RadiusB, 2);
}

bool FPhysicsInterface::SweepSphereSphere(const FVector& Start, const FVector& Delta, float RadiusA, 
										  const FVector& CenterB, float RadiusB, FHitResult& OutHit)
{
	//P(t) = S + tD
	//		S에서 D방향으로 t만큼
	// R = RA + RB
	// |P(t) - Cb| = R 일 때 닿았다고 판단
	// 양변에 제곱하면
	// (S + tD - Cb)^2 = R^2
	// m = S - Cb라고 할 때 S = m + Cb를 대입하면
	// (m + tD)^2 - R^2 = 0
	// D^2t^2 + 2Dmt + m^2 - R^2 = 0
	// 근의 공식으로 t 두개를 구하고 더 작은 쪽이 가장 가까운 표면
	// 근의 공식
	//		t = (-b +- sqrt(b^2 - 4ac)) / 2a
	// 짝수 공식
	//		b = 2b' -> b' = Dm
	//		t = (-b' +- sqrt(b'^2 - ac)) / a

	//큰 원의 반지름
	float R = RadiusA + RadiusB;

	// D^2t^2 + 2Dmt + m^2 - R^2 = 0 -> at^2 + bt + c = 0
	float a = Delta.Dot(Delta);
	FVector m = Start - CenterB;
	float b = m.Dot(Delta);
	float c = m.Dot(m) - R * R;

	//a = 0
	//	이동 없음. a로 나누면 안됨(0 나누기임)
	if (a < Epsilon)
	{
		return false;
	}
	
	//닿았거나 아직 닿지 않았는데 바깥으로 가고있다.
	if (c >= 0 && b >= 0)
	{
		return false;
	}

	//c < 0
	//	m^2 - R^2 가 0보다 작다면 이미 파고든채 시작함
	//	Time = 0, bStartPentrating = true, PenetrationDepth = R - |m|, Normal = m 정규화
	if (c < 0 && b < 0)
	{
		OutHit.Time = 0.f;
		OutHit.bBlockingHit = true;
		OutHit.bStartPentrating = true;
		OutHit.PenetrationDepth = R - m.Size();
		OutHit.Normal = m.GetSafeNormal();
		OutHit.Location = Start;
		return true;
	}

	//겹친 상태인데 빠져나가고 있음
	if (c < 0 && b >= 0)
	{
		return false;
	}

	//닿아 있는채로 시작해서 안쪽으로 간다.
	if (0 <= c && c < Epsilon && b < 0)
	{
		OutHit.Time = 0.f;
		OutHit.bBlockingHit = true;
		OutHit.bStartPentrating = true;
		OutHit.PenetrationDepth = R - m.Size();
		OutHit.Normal = m.GetSafeNormal();
		OutHit.Location = Start;
		return true;
	}

	//b^2 - ac : 짝수 공식 루트 안쪽
	float s = b * b - a * c;

	//비껴감
	if (s < 0)
	{
		return false;
	}
	

	//근의 공식 중 작은 쪽
	float t = (- b - std::sqrt(s)) / a;

	//언젠가 닿긴 할텐데 이번 이동에서는 닿지 않음
	if (t > 1)
	{
		return false;
	}

	//Hit 없음
	if (t < 0)
	{
		return false;
	}
	
	//Hit
	// t = 0이면 스침. t 값이 1개 밖에 없는것.
	if (0 <= t && t <= 1)
	{
		OutHit.Time = t;
		OutHit.Distance = Delta.Size() * t;
		FVector Location = Start + Delta * t;
		OutHit.Location = Location;
		FVector Normal = (Location - CenterB) / R;
		OutHit.Normal = Normal;
		OutHit.ImpactNormal = Normal;
		OutHit.ImpactPoint = CenterB + Normal * RadiusB;
		OutHit.bBlockingHit = true;
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = Start + Delta;
		return true;
	}

	return false;
}

bool FPhysicsInterface::SphereBox(const FVector& CenterSphere, float RadiusSphere, const FVector& CenterBox, const FVector& BoxExtent, const FRotator& BoxRot)
{
	//박스 위 표면에서 구의 중심과 가장 가까운 점 Q를 찾는다
	// 구에서 Q까지의 거리 < 반지름 : 겹침
	
	//구를 박스의 로컬 좌표계로 보낸다
	//구의 위치를 박스의 Extent로 clamp하면 점 Q가 구해진다.
	//Q와 구의 중심으로부터 반지름과 비교

	/*const FQuat Q = BoxRot.Quaternion();
	const FVector Axes[3] =
	{
		Q.RotateVector(FVector::ForwardVector),
		Q.RotateVector(FVector::RightVector),
		Q.RotateVector(FVector::UpVector)
	};*/
	FVector Axes[3] = {FVector(), FVector(), FVector()};

	GetBoxAxes(BoxRot, Axes);

	FVector D = CenterSphere - CenterBox;
	
	//박스 로컬 좌표계로 변환
	float Px = D.Dot(Axes[0]);
	float Py = D.Dot(Axes[1]);
	float Pz = D.Dot(Axes[2]);

	//박스의 Extent로 clamp
	float Qx = FMath::Clamp(Px, -BoxExtent.X, BoxExtent.X);
	float Qy = FMath::Clamp(Py, -BoxExtent.Y, BoxExtent.Y);
	float Qz = FMath::Clamp(Pz, -BoxExtent.Z, BoxExtent.Z);

	FVector d = FVector(Px - Qx, Py - Qy, Pz - Qz);
	return d.Dot(d) < (RadiusSphere * RadiusSphere);
}

bool FPhysicsInterface::SphereCapsule(const FVector& CenterSphere, float RadiusSphere, const FVector& CenterCapsule, float RadiusCapsule, float HalfHeightCapsule, const FRotator& CapsuleRot)
{
	//캡슐은 두께가 있는 선분이라고 생각한다
	// 선분 P0 - P1에서 거리가 R이하인 모든 점의 집합이다.
	//선분 위에서 구 중심 C와 가장 가까운 점 Q를 찾는다.
	//|C-Q|^2 < (r + R)^2 일때 겹침
	//	r은 구의 반지름
	
	//선분 만들기
	FVector P0;
	FVector P1;
	GetCapsuleSegment(CenterCapsule, CapsuleRot, HalfHeightCapsule, RadiusCapsule, P0, P1);

	//가까운 점 Q
	FVector Q = ClosestPointOnSegment(CenterSphere, P0, P1);

	float RadiusSum = RadiusSphere + RadiusCapsule;

	return (CenterSphere - Q).SizeSquared() < RadiusSum * RadiusSum;
}

bool FPhysicsInterface::CapsuleCapsule(const FVector& CenterA, const FRotator& RotA, float HalfHeightA, float RadiusA, 
									   const FVector& CenterB, const FRotator& RotB, float HalfHeightB, float RadiusB)
{
	FVector P1;
	FVector Q1;
	FVector P2;
	FVector Q2;
	//두 캡슐 시작, 끝 지점
	GetCapsuleSegment(CenterA, RotA, HalfHeightA, RadiusA, P1, Q1);
	GetCapsuleSegment(CenterB, RotB, HalfHeightB, RadiusB, P2, Q2);

	FVector C1;
	FVector C2;
	//가장 가까운 지점 C1, C2, 두 지점 거리의 제곱 반환
	float DistSq = ClosesPointsBetweenSegments(P1, Q1, P2, Q2, C1, C2);

	float RadiusSum = RadiusA + RadiusB;

	return DistSq < RadiusSum * RadiusSum;
}

bool FPhysicsInterface::BoxBox(const FVector& CenterA, const FVector& ExtentA, const FRotator& RotA, const FVector& CenterB, const FVector& ExtentB, const FRotator& RotB)
{
	//두 박스의 3축
	FVector AAxes[3] = { FVector(), FVector(), FVector() };
	FVector BAxes[3] = { FVector(), FVector(), FVector() };
	GetBoxAxes(RotA, AAxes);
	GetBoxAxes(RotB, BAxes);

	FVector T = CenterB - CenterA;

	FVector CalcAxes[15] = {};
	for (int i = 0; i < std::size(AAxes); ++i)
	{
		CalcAxes[i] = AAxes[i];
	}

	for (int i = 0; i < std::size(BAxes); ++i)
	{
		CalcAxes[3 + i] = BAxes[i];
	}

	//각 3개 축끼리 외적한 면도 추가
	//모서리 9개에 대해서 검사
	for (int i = 0; i < std::size(AAxes); ++i)
	{
		for (int j = 0; j < std::size(BAxes); ++j)
		{
			CalcAxes[6 + (i * 3) + j] = AAxes[i].Cross(BAxes[j]);
		}
	}

	//15개 축에 대하여 검사
	//하나라도 틈이 있다면 겹치지 않음
	//모두 틈이 없다면 겹침
	for (int i = 0; i < std::size(CalcAxes); ++i)
	{
		FVector L = CalcAxes[i];
		//0벡터 걸러내기
		if (L.SizeSquared() < Epsilon) continue;

		if (IsSeparatedOnAxis(L, T, AAxes, ExtentA, BAxes, ExtentB))
		{
			return false;
		}
	}

	return true;
}

FBox FPhysicsInterface::CalcShapeBounds(const FCollisionShape& Shape, const FVector& Pos, const FRotator& Rot)
{
	switch (Shape.ShapeType)
	{
	case ECollisionShape::Sphere:
	{
		const FVector R(Shape.SphereRadius, Shape.SphereRadius, Shape.SphereRadius);
		return FBox{ Pos - R, Pos + R };
	}
	case ECollisionShape::Box:
	{
		//회전된 세 축을 Extent만큼 늘린 뒤 축별 절댓값을 더하면 월드 축 반지름이 된다
		FVector Axes[3];
		GetBoxAxes(Rot, Axes);

		const FVector& Ext = Shape.BoxExtent;
		const FVector E(
			std::fabs(Axes[0].X) * Ext.X + std::fabs(Axes[1].X) * Ext.Y + std::fabs(Axes[2].X) * Ext.Z,
			std::fabs(Axes[0].Y) * Ext.X + std::fabs(Axes[1].Y) * Ext.Y + std::fabs(Axes[2].Y) * Ext.Z,
			std::fabs(Axes[0].Z) * Ext.X + std::fabs(Axes[1].Z) * Ext.Y + std::fabs(Axes[2].Z) * Ext.Z);
		return FBox{ Pos - E, Pos + E };
	}
	case ECollisionShape::Capsule:
	{
		//몸통 선분 양 끝을 감싸는 박스를 반지름만큼 늘린다
		FVector P0;
		FVector P1;
		GetCapsuleSegment(Pos, Rot, Shape.CapsuleHalfHeight, Shape.CapsuleRadius, P0, P1);

		const FVector R(Shape.CapsuleRadius, Shape.CapsuleRadius, Shape.CapsuleRadius);
		const FVector Min(std::min(P0.X, P1.X), std::min(P0.Y, P1.Y), std::min(P0.Z, P1.Z));
		const FVector Max(std::max(P0.X, P1.X), std::max(P0.Y, P1.Y), std::max(P0.Z, P1.Z));
		return FBox{ Min - R, Max + R };
	}
	default:
		return FBox{ Pos, Pos };
	}
}

void FPhysicsInterface::GetCapsuleSegment(const FVector& Center, const FRotator& Rot, float HalfHeight, float Radius, FVector& OutP0, FVector& OutP1)
{
	//캡슐 로컬 Up 벡터
	FVector Up = Rot.Quaternion().RotateVector(FVector::UpVector);

	//선분의 길이(반구 제외) : 몸통 부분 길이
	//	반높이가 반지름보다 작게 들어와도 음수가 되지 않게 한다 (그때는 구와 같다)
	float h = std::max(HalfHeight - Radius, 0.f);

	OutP0 = Center - Up * h;
	OutP1 = Center + Up * h;
}

FVector FPhysicsInterface::ClosestPointOnSegment(const FVector& Point, const FVector& P0, const FVector& P1)
{
	//선분의 방향벡터
	FVector d = P1 - P0;

	float t = 0.f;
	const float DD = d.Dot(d);

	if (DD < Epsilon)
	{
		t = 0.f;
	}
	else
	{
		//P0에서 선분 위로 Point를 투영한 점 Q까지의 길이를 0.0  ~ 1.0 비율로
		FVector v = Point - P0;
		//d가 단위 벡터일 때 내적 값이 투영 길이와 같다
		//	투영 길이 = v dot d / |d|
		//	투영 길이를 d의 크기로 다시 나눠서 0 ~ 1의 비율로 한다
		//	v dot d / |d| / |d|
		//	= v dot d / |d|^2
		//	= v dot d / d dot d(자기 자신을 내적하면 길이의 제곱이다)
		t = v.Dot(d) / d.Dot(d);
	}

	//선분 내부로 클램프 : 바깥쪽으로 나갔다면 가장 끝점으로 클램프
	t = FMath::Clamp(t, 0.f, 1.f);

	return P0 + d * t;
}

float FPhysicsInterface::ClosesPointsBetweenSegments(const FVector& P1, const FVector& Q1, 
													 const FVector& P2, const FVector& Q2, FVector& OutC1, FVector& OutC2)
{
	//선분 1, 2 방향 + 길이
	FVector d1 = Q1 - P1;
	FVector d2 = Q2 - P2;
	
	//두 시작점 사이
	FVector r = P1 - P2;
	
	//a = d1^2
	const float a = d1.Dot(d1);

	//e = d2^2
	const float e = d2.Dot(d2);

	//f = d2 * r
	const float f = d2.Dot(r);

	const float b = d1.Dot(d2);

	const float c = d1.Dot(r);

	//s는 선분 1에서의 비율
	//t는 선분 2에서의 비율
	float s = 0.f;
	float t = 0.f;

	//t = (bs + f) / e
	//s = (bf - ce)/(ae - b^2)

	//둘 다 점
	if (a <= Epsilon && e <= Epsilon)
	{
		/*OutC1 = P1;
		OutC2 = P2;
		return r.SizeSquared();*/
	}
	//1만 점
	else if (a <= Epsilon)
	{
		s = 0.f;
		t = FMath::Clamp(f / e, 0.f, 1.f);		
	}
	//2만 점
	else if (e <= Epsilon)
	{
		t = 0.f;
		s = FMath::Clamp(-c / a, 0.f, 1.f);
	}
	//둘 다 선분
	else
	{
		//분모
		const float denom = a * e - b * b;

		//대입법으로 구한 s를 [0,1]로. 평행이면 s = 0에서 시작
		s = (denom > Epsilon * a * e) ? FMath::Clamp((b * f - c * e) / denom, 0.f, 1.f) : 0.f;

		//그 s에 가장 가까운 t
		t = (b * s + f) / e;

		// t가 넘치면 t를 끝으로 고정하고 s를 다시
		if (t < 0.f)
		{
			t = 0.f;
			s = FMath::Clamp(-c / a, 0.f, 1.f);
		}
		else if (t > 1.f)
		{
			t = 1.f;
			s = FMath::Clamp((b - c) / a, 0.f, 1.f);
		}
	}

	OutC1 = P1 + d1 * s;
	OutC2 = P2 + d2 * t;
	return (OutC1 - OutC2).SizeSquared();
}

void FPhysicsInterface::GetBoxAxes(const FRotator& Rot, FVector OutAxes[3])
{
	const FQuat Q = Rot.Quaternion();
	OutAxes[0] = Q.RotateVector(FVector::ForwardVector);
	OutAxes[1] = Q.RotateVector(FVector::RightVector);
	OutAxes[2] = Q.RotateVector(FVector::UpVector);
}

bool FPhysicsInterface::IsSeparatedOnAxis(const FVector& L, const FVector& T, const FVector AxesA[3], const FVector& ExtentA, const FVector AxesB[3], const FVector& ExtentB)
{
	const float Dist = std::fabs(T.Dot(L));
	const float RA = ExtentA.X * std::fabs(AxesA[0].Dot(L)) + 
					ExtentA.Y * std::fabs(AxesA[1].Dot(L)) + 
					ExtentA.Z * std::fabs(AxesA[2].Dot(L));
	const float RB = ExtentB.X * std::fabs(AxesB[0].Dot(L)) + ExtentB.Y * std::fabs(AxesB[1].Dot(L)) + ExtentB.Z * std::fabs(AxesB[2].Dot(L));
	return Dist >= RA + RB;
}

bool FPhysicsInterface::ComputePenetration(const FCollisionShape& ShapeA, const FVector& PosA, const FRotator& RotA, 
										   const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB, 
										   FVector& OutNormal, float& OutDepth)
{
	ECollisionShape TypeA = ShapeA.ShapeType;
	ECollisionShape TypeB = ShapeB.ShapeType;

	// 정해진 순서로만 계산하고, 순서를 바꿔 호출한 경우 normal을 뒤집는다

	if (TypeA == ECollisionShape::Sphere && TypeB == ECollisionShape::Sphere)
	{		
		// 스피어 스피어
		return PenFromClosestPoints(PosA, PosB, ShapeA.SphereRadius + ShapeB.SphereRadius, OutNormal, OutDepth);
	}

	float SphereRadiusA = ShapeA.SphereRadius;
	float SphereRadiusB = ShapeB.SphereRadius;

	FVector BoxExtentA = ShapeA.BoxExtent;
	FVector BoxExtentB = ShapeB.BoxExtent;

	float CapsuleRadiusA = ShapeA.CapsuleRadius;
	float CapsuleRadiusB = ShapeB.CapsuleRadius;

	float CapsuleHalfHeightA = ShapeA.CapsuleHalfHeight;
	float CapsuleHalfHeightB = ShapeB.CapsuleHalfHeight;

	//스피어 - 박스
	if (TypeA == ECollisionShape::Sphere && TypeB == ECollisionShape::Box)
	{
		return PenSphereBox(PosA, ShapeA.SphereRadius, PosB, ShapeB.BoxExtent, RotB, OutNormal, OutDepth);
	}

	//박스 - 스피어
	if (TypeA == ECollisionShape::Box && TypeB == ECollisionShape::Sphere)
	{
		// 노멀 뒤집기
		const bool bHit = PenSphereBox(PosB, ShapeB.SphereRadius, PosA, ShapeA.BoxExtent, RotA, OutNormal, OutDepth);
		OutNormal = OutNormal * -1.f;
		return bHit;
	}

	// 스피어 캡슐
	if (TypeA == ECollisionShape::Sphere && TypeB == ECollisionShape::Capsule)
	{
		FVector P0, P1;
		GetCapsuleSegment(PosB, RotB, CapsuleHalfHeightB, CapsuleRadiusB, P0, P1);
		const FVector Q = ClosestPointOnSegment(PosA, P0, P1);
		return PenFromClosestPoints(PosA, Q, SphereRadiusA + CapsuleRadiusB, OutNormal, OutDepth);
	}

	// 캡슐 스피어
	if (TypeA == ECollisionShape::Capsule && TypeB == ECollisionShape::Sphere)
	{
		FVector P0, P1;
		GetCapsuleSegment(PosA, RotA, CapsuleHalfHeightA, CapsuleRadiusA, P0, P1);
		const FVector Q = ClosestPointOnSegment(PosB, P0, P1);
		const bool bHit = PenFromClosestPoints(PosB, Q, SphereRadiusB + CapsuleRadiusA, OutNormal, OutDepth);
		OutNormal = OutNormal * -1.f;
		return bHit;
	}

	if (TypeA == ECollisionShape::Capsule && TypeB == ECollisionShape::Capsule)
	{
		FVector PA0;
		FVector PA1;
		FVector PB0;
		FVector PB1;
		// 캡슐 A – 캡슐 B
		GetCapsuleSegment(PosA, RotA, CapsuleHalfHeightA, CapsuleRadiusA, PA0, PA1);
		GetCapsuleSegment(PosB, RotB, CapsuleHalfHeightB, CapsuleRadiusB, PB0, PB1);

		FVector C1, C2;
		ClosesPointsBetweenSegments(PA0, PA1, PB0, PB1, C1, C2);
		if (!PenFromClosestPoints(C1, C2, CapsuleRadiusA + CapsuleRadiusB, OutNormal, OutDepth))
		{
			return false;
		}

		// 두 뼈대가 정확히 교차하면 C1 == C2라 방향이 없다(임의로 Up이 들어감).
		//	두 선분에 모두 수직인 방향으로 바꾸고, A가 B에서 멀어지는 쪽을 고른다.
		if ((C1 - C2).SizeSquared() <= Epsilon)
		{
			FVector Perp = (PA1 - PA0).Cross(PB1 - PB0);
			if (Perp.SizeSquared() > Epsilon)
			{
				Perp = Perp.GetSafeNormal();
				OutNormal = (Perp.Dot(PosA - PosB) < 0.f) ? Perp * -1.f : Perp;
			}
		}
		return true;
	}
	
	// 박스 박스
	if (TypeA == ECollisionShape::Box && TypeB == ECollisionShape::Box)
	{
		return PenBoxBox(PosA, BoxExtentA, RotA, PosB, BoxExtentB, RotB, OutNormal, OutDepth);
	}

	// 캡슐 박스
	if (TypeA == ECollisionShape::Capsule && TypeB == ECollisionShape::Box)
	{
		return PenCapsuleBox(PosA, RotA, CapsuleHalfHeightA, CapsuleRadiusA, PosB, BoxExtentB, RotB, OutNormal, OutDepth);
	}

	// 박스 캡슐: 캡슐을 앞에 두고 계산 -> 결과는 박스(A) -> 캡슐(B) 방향이라 뒤집는다
	if (TypeA == ECollisionShape::Box && TypeB == ECollisionShape::Capsule)
	{
		const bool bHit = PenCapsuleBox(PosB, RotB, CapsuleHalfHeightB, CapsuleRadiusB, PosA, BoxExtentA, RotA, OutNormal, OutDepth);
		OutNormal = OutNormal * -1.f;
		return bHit;
	}

	return false;
}

float FPhysicsInterface::GetThinScale(const FCollisionShape& Shape)
{
	switch (Shape.ShapeType)
	{
		case ECollisionShape::None:
			break;
		case ECollisionShape::Sphere:
			return Shape.SphereRadius;
			break;
		case ECollisionShape::Box:
		{
			FVector Extent = Shape.BoxExtent;
			return std::min({ Extent.X, Extent.Y, Extent.Z });
			break;
		}
		case ECollisionShape::Capsule:
			return Shape.CapsuleRadius;
			break;
		default:
			break;
	}
	return 0.f;
}

void FPhysicsInterface::FillHitResult(const FCollisionShape& ShapeA, const FVector& Start, const FVector& Delta, const FRotator& RotA, const FCollisionShape& ShapeB, const FVector& PosB, const FRotator& RotB, float lo, float hi, FHitResult& OutHit)
{
	const FVector StopPos = Start + Delta * lo;

	FVector Normal; float Depth;
	if (!ComputePenetration(ShapeA, Start + Delta * hi, RotA, ShapeB, PosB, RotB, Normal, Depth))
	{
		// 아직 normal 계산이 없는 쌍: 이동 반대 방향으로 대신한다
		Normal = (Delta * -1.f).GetSafeNormal();
	}

	OutHit.bBlockingHit = true;
	OutHit.Time = lo;
	OutHit.Distance = Delta.Size() * lo;
	OutHit.Location = StopPos;
	OutHit.Normal = Normal;
	OutHit.ImpactNormal = Normal;
	OutHit.ImpactPoint = StopPos - Normal * GetThinScale(ShapeA);   // 근사
	OutHit.TraceStart = Start;
	OutHit.TraceEnd = Start + Delta;
}

bool FPhysicsInterface::PenFromClosestPoints(const FVector& PA, const FVector& PB, float RadiusSum, FVector& OutNormal, float& OutDepth)
{
	// B -> A
	const FVector Diff = PA - PB;
	const float DistSq = Diff.SizeSquared();

	// OverlapTest와 같은 기준 (닿기만 하면 겹침 아님)
	if (DistSq >= RadiusSum * RadiusSum)
		return false;

	const float Dist = std::sqrt(DistSq);
	// 뼈대가 정확히 만나면 방향을 정할 수 없어 임의로
	OutNormal = (Dist > Epsilon) ? Diff / Dist : FVector::UpVector;
	// 얼마나 파고들었나
	OutDepth = RadiusSum - Dist;
	return true;
}

bool FPhysicsInterface::PenSphereBox(const FVector& CenterSphere, float Radius, 
									 const FVector& CenterBox, const FVector& Extent, const FRotator& BoxRot, 
									 FVector& OutNormal, float& OutDepth)
{
	FVector Axes[3];
	GetBoxAxes(BoxRot, Axes);

	// 구 중심을 박스 로컬 좌표로 (SphereBox와 같음)
	const FVector D = CenterSphere - CenterBox;
	const float P[3] = { D.Dot(Axes[0]), D.Dot(Axes[1]), D.Dot(Axes[2]) };
	const float E[3] = { Extent.X, Extent.Y, Extent.Z };

	// 박스 안으로 clamp -> 박스 위 최근접점 (다시 월드로)
	float Q[3];
	for (int32 i = 0; i < 3; ++i) Q[i] = FMath::Clamp(P[i], -E[i], E[i]);
	const FVector Closest = CenterBox + Axes[0] * Q[0] + Axes[1] * Q[1] + Axes[2] * Q[2];

	const FVector Diff = CenterSphere - Closest;
	const float DistSq = Diff.SizeSquared();

	// 구 중심이 박스 밖
	if (DistSq > Epsilon)
	{
		if (DistSq >= Radius * Radius) return false;
		const float Dist = std::sqrt(DistSq);
		OutNormal = Diff / Dist;
		OutDepth = Radius - Dist;
		return true;
	}

	// 구 중심이 박스 안: clamp해도 그대로라 Diff = 0 : 방향이 없다
	//      가장 가까운 면으로 밀어낸다
	int32 Best = 0;
	// 그 면까지 남은 거리
	float BestGap = E[0] - std::fabs(P[0]);
	for (int32 i = 1; i < 3; ++i)
	{
		const float Gap = E[i] - std::fabs(P[i]);
		if (Gap < BestGap) { BestGap = Gap; Best = i; }
	}
	const float Sign = (P[Best] >= 0.f) ? 1.f : -1.f;
	// 그 면의 바깥 방향
	OutNormal = Axes[Best] * Sign;
	// 면까지 + 반지름만큼 밀어야 빠져나옴
	OutDepth = Radius + BestGap;
	return true;
}

float FPhysicsInterface::ProjectedOverlap(const FVector& L, const FVector& T, const FVector AxesA[3], const FVector& ExtentA, const FVector AxesB[3], const FVector& ExtentB)
{
	const float Dist = std::fabs(T.Dot(L));
	const float RA = ExtentA.X * std::fabs(AxesA[0].Dot(L)) + ExtentA.Y * std::fabs(AxesA[1].Dot(L)) + ExtentA.Z * std::fabs(AxesA[2].Dot(L));
	const float RB = ExtentB.X * std::fabs(AxesB[0].Dot(L)) + ExtentB.Y * std::fabs(AxesB[1].Dot(L)) + ExtentB.Z * std::fabs(AxesB[2].Dot(L));
	return (RA + RB) - Dist;
}

bool FPhysicsInterface::PenBoxBox(const FVector& CenterA, const FVector& ExtentA, const FRotator& RotA, const FVector& CenterB, const FVector& ExtentB, const FRotator& RotB, FVector& OutNormal, float& OutDepth)
{
	//두 박스의 3축
	FVector AAxes[3] = { FVector(), FVector(), FVector() };
	FVector BAxes[3] = { FVector(), FVector(), FVector() };
	GetBoxAxes(RotA, AAxes);
	GetBoxAxes(RotB, BAxes);

	FVector T = CenterB - CenterA;

	FVector CalcAxes[15] = {};
	for (int i = 0; i < std::size(AAxes); ++i)
	{
		CalcAxes[i] = AAxes[i];
	}

	for (int i = 0; i < std::size(BAxes); ++i)
	{
		CalcAxes[3 + i] = BAxes[i];
	}

	//각 3개 축끼리 외적한 면도 추가
	//모서리 9개에 대해서 검사
	for (int i = 0; i < std::size(AAxes); ++i)
	{
		for (int j = 0; j < std::size(BAxes); ++j)
		{
			CalcAxes[6 + (i * 3) + j] = AAxes[i].Cross(BAxes[j]);
		}
	}

	float BestDepth = FLT_MAX;
	FVector BestAxis = FVector::UpVector;

	for (int32 i = 0; i < 15; ++i)
	{
		// 평행 모서리
		if (CalcAxes[i].SizeSquared() < Epsilon) continue;

		// 여기서는 정규화 필요
		const FVector L = CalcAxes[i].GetSafeNormal();          
		const float Overlap = ProjectedOverlap(L, T, AAxes, ExtentA, BAxes, ExtentB);

		// 틈 있음 -> 안 겹침
		if (Overlap <= 0.f) return false;                       

		if (Overlap < BestDepth) { BestDepth = Overlap; BestAxis = L; }
	}

	// B -> A 방향: T(A->B)와 같은 쪽이면 뒤집는다
	OutNormal = (T.Dot(BestAxis) > 0.f) ? BestAxis * -1.f : BestAxis;
	OutDepth = BestDepth;
	return true;
}

FVector FPhysicsInterface::ClosestPointOnBox(const FVector& Point, const FVector& BoxCenter, const FVector BoxAxes[3], const FVector& BoxExtent)
{
	// 박스 로컬 좌표로 옮겨 Extent로 clamp한 뒤 다시 월드로 (SphereBox와 같은 방식)
	const FVector D = Point - BoxCenter;
	const float E[3] = { BoxExtent.X, BoxExtent.Y, BoxExtent.Z };

	FVector Result = BoxCenter;
	for (int32 i = 0; i < 3; ++i)
	{
		const float Local = FMath::Clamp(D.Dot(BoxAxes[i]), -E[i], E[i]);
		Result = Result + BoxAxes[i] * Local;
	}
	return Result;
}

float FPhysicsInterface::ClosestPointsSegmentBox(const FVector& P0, const FVector& P1,
												 const FVector& BoxCenter, const FVector BoxAxes[3], const FVector& BoxExtent,
												 FVector& OutOnSegment, FVector& OutOnBox)
{
	// 선분 위 점 P(t) = P0 + t·d 에서 박스까지 거리²를 f(t)라 하면,
	// 박스가 볼록이라 f(t)는 골짜기가 하나인 함수다. 삼분 탐색으로 가장 낮은 t를 찾는다.
	const FVector d = P1 - P0;

	auto DistSqAt = [&](float t, FVector& OutPoint, FVector& OutBoxPoint)
	{
		OutPoint = P0 + d * t;
		OutBoxPoint = ClosestPointOnBox(OutPoint, BoxCenter, BoxAxes, BoxExtent);
		return (OutPoint - OutBoxPoint).SizeSquared();
	};

	// 선분 길이가 0이면 점 하나 (구와 같은 경우)
	if (d.SizeSquared() <= Epsilon)
	{
		return DistSqAt(0.f, OutOnSegment, OutOnBox);
	}

	float Lo = 0.f;
	float Hi = 1.f;
	FVector PointA, BoxA, PointB, BoxB;

	// 구간을 매번 2/3로 줄인다. 40번이면 (2/3)^40 ≈ 1e-7
	for (int32 Iter = 0; Iter < 40; ++Iter)
	{
		const float M1 = Lo + (Hi - Lo) / 3.f;
		const float M2 = Hi - (Hi - Lo) / 3.f;
		if (DistSqAt(M1, PointA, BoxA) <= DistSqAt(M2, PointB, BoxB))
		{
			// 더 낮은 쪽이 M1 → 최솟값은 M2보다 앞
			Hi = M2;
		}
		else
		{
			Lo = M1;
		}
	}

	return DistSqAt((Lo + Hi) * 0.5f, OutOnSegment, OutOnBox);
}

bool FPhysicsInterface::CapsuleBox(const FVector& CenterCapsule, const FRotator& CapsuleRot, float HalfHeight, float Radius,
								   const FVector& CenterBox, const FVector& BoxExtent, const FRotator& BoxRot)
{
	// 캡슐 = 두께 R인 선분. 선분과 박스 사이 최단 거리 < R 이면 겹침
	FVector P0, P1;
	GetCapsuleSegment(CenterCapsule, CapsuleRot, HalfHeight, Radius, P0, P1);

	FVector Axes[3];
	GetBoxAxes(BoxRot, Axes);

	FVector OnSegment, OnBox;
	const float DistSq = ClosestPointsSegmentBox(P0, P1, CenterBox, Axes, BoxExtent, OnSegment, OnBox);
	return DistSq < Radius * Radius;
}

bool FPhysicsInterface::PenCapsuleBox(const FVector& CenterCapsule, const FRotator& CapsuleRot, float HalfHeight, float Radius,
									  const FVector& CenterBox, const FVector& BoxExtent, const FRotator& BoxRot,
									  FVector& OutNormal, float& OutDepth)
{
	FVector P0, P1;
	GetCapsuleSegment(CenterCapsule, CapsuleRot, HalfHeight, Radius, P0, P1);

	FVector Axes[3];
	GetBoxAxes(BoxRot, Axes);

	FVector OnSegment, OnBox;
	const float DistSq = ClosestPointsSegmentBox(P0, P1, CenterBox, Axes, BoxExtent, OnSegment, OnBox);
	if (DistSq >= Radius * Radius)
	{
		return false;
	}

	// (가) 뼈대가 박스 밖: 최근접점 방식 (박스 위 점 -> 선분 위 점)
	if (DistSq > Epsilon)
	{
		return PenFromClosestPoints(OnSegment, OnBox, Radius, OutNormal, OutDepth);
	}

	// (나) 뼈대가 박스 안을 지남: 최근접점이 같아 방향이 없다.
	//	박스 면 축 3개와 (캡슐 축 × 박스 축) 3개에 투영해서 가장 적게 겹친 축으로 밀어낸다 (Box–Box와 같은 생각)
	const FVector Seg = P1 - P0;
	const float SegLen = Seg.Size();
	const FVector Dir = (SegLen > Epsilon) ? Seg / SegLen : FVector::UpVector;
	const float HalfSeg = SegLen * 0.5f;
	const FVector CapsuleMid = (P0 + P1) * 0.5f;
	const FVector T = CenterBox - CapsuleMid;     // A(캡슐) -> B(박스)

	FVector CandAxes[6];
	int32 NumAxes = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		CandAxes[NumAxes++] = Axes[i];
	}
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector Cross = Dir.Cross(Axes[i]);
		if (Cross.SizeSquared() > Epsilon)
		{
			CandAxes[NumAxes++] = Cross.GetSafeNormal();
		}
	}

	float BestDepth = FLT_MAX;
	FVector BestAxis = FVector::UpVector;
	for (int32 i = 0; i < NumAxes; ++i)
	{
		const FVector& L = CandAxes[i];
		// 캡슐 그림자 반길이 = 선분 반길이의 투영 + 반지름
		const float RCapsule = HalfSeg * std::fabs(Dir.Dot(L)) + Radius;
		const float RBox = BoxExtent.X * std::fabs(Axes[0].Dot(L)) + BoxExtent.Y * std::fabs(Axes[1].Dot(L)) + BoxExtent.Z * std::fabs(Axes[2].Dot(L));
		const float Overlap = RCapsule + RBox - std::fabs(T.Dot(L));
		if (Overlap > 0.f && Overlap < BestDepth)
		{
			BestDepth = Overlap;
			BestAxis = L;
		}
	}

	// B(박스) -> A(캡슐): T(A->B)와 같은 쪽이면 뒤집는다
	OutNormal = (T.Dot(BestAxis) > 0.f) ? BestAxis * -1.f : BestAxis;
	OutDepth = (BestDepth < FLT_MAX) ? BestDepth : Radius;
	return true;
}
