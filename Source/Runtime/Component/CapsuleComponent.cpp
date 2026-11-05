#include "EnginePCH.h"
#include "CapsuleComponent.h"
#include "Render/LineBatcher.h"

UCapsuleComponent::UCapsuleComponent()
{

}

UCapsuleComponent::~UCapsuleComponent()
{

}

void UCapsuleComponent::BeginPlay()
{
	Super::BeginPlay();
}


//bool UCapsuleComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
//{
//	const FStaticMeshData* Mesh = GetMeshData();
//	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
//}

//bool UCapsuleComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
//{
//	const FStaticMeshData* Mesh = GetMeshData();
//	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
//}

void UCapsuleComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
}

FCollisionShape UCapsuleComponent::GetCollisionShape() const
{
	return FCollisionShape::MakeCapsule(GetWorldCapsuleRadius(), GetWorldCapsuleHalfHeight());
}

void UCapsuleComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	//캡슐 = 원통 + 위아래 반구
	//위 원, 아래 원
	//	수평 원 2개
	//세로선 4개
	//	두 원을 +-Right, +-Forward 위치에서 연결
	//위 반구 호 2개, 아래 반구 호 2개
	//	Right-Up 평면, Forwrd-Up 평면

	FVector C = GetWorldLocation();
	FMatrix M = GetWorldMatrix();
	FVector Forward = M.TransformVectorNoScale(FVector::ForwardVector);
	FVector Right = M.TransformVectorNoScale(FVector::RightVector);
	FVector Up = M.TransformVectorNoScale(FVector::UpVector);
	FVector S = GetWorldScale3D();
	float R = GetWorldCapsuleRadius();
	//R을 넘을 순 없다
	float HH = GetWorldCapsuleHalfHeight();
	float h = HH - R;
	FVector Top = C + Up * h;
	FVector Bottom = C - Up * h;

	const FVector Axes[4] = { Right, -Right, Forward, -Forward };

	//위 아래 원
	DrawArc(LineBatcher, Top, Right, Forward, R, 0, 2 * PI, 32, GetDebugColor());
	DrawArc(LineBatcher, Bottom, Right, Forward, R, 0, 2 * PI, 32, GetDebugColor());
	
	//원에 선 잇기
	for (int i = 0; i < 4; ++i)
	{
		LineBatcher->AddLine(Bottom + Axes[i] * R, Top + Axes[i] * R, GetDebugColor());
	}

	//위 아래 반구
	DrawArc(LineBatcher, Top, Right, Up, R, 0, PI, 32, GetDebugColor());
	DrawArc(LineBatcher, Top, Forward, Up, R, 0, PI, 32, GetDebugColor());
	DrawArc(LineBatcher, Bottom, Right, -Up, R, 0, PI, 32, GetDebugColor());
	DrawArc(LineBatcher, Bottom, Forward, -Up, R, 0, PI, 32, GetDebugColor());
}

float UCapsuleComponent::GetWorldCapsuleRadius() const
{
	FVector S = GetWorldScale3D();
	return CapsuleRadius * std::min(std::fabs(S.X), std::fabs(S.Y));
}

float UCapsuleComponent::GetWorldCapsuleHalfHeight() const
{
	FVector S = GetWorldScale3D();
	float R = GetWorldCapsuleRadius();
	return std::max(CapsuleHalfHeight * std::fabs(S.Z), R);
}

FBox UCapsuleComponent::CalcLocalBounds() const
{
	// 반높이는 반구까지 포함한 값이다. 반지름보다 작으면 구와 같다 (GetWorldCapsuleHalfHeight와 같은 규칙)
	FVector E = FVector(CapsuleRadius, CapsuleRadius, std::max(CapsuleHalfHeight, CapsuleRadius));
	return FBox(-E, E);
}
