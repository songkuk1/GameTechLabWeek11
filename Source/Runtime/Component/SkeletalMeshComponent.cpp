#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::PostPhysics;
	AnimInstance = NewObject<UAnimationInstance>(this);
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
	delete AnimInstance;
}

void USkeletalMeshComponent::SetAnimationInstance(UAnimationInstance* InInstance)
{
	if (InInstance) AnimInstance = InInstance;
	ResetPose();
	UpdateBoneMatrices();
}

const FModelMeshData* USkeletalMeshComponent::GetMeshData() const
{
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	return &SkeletalMesh->GetMeshData();
}

void USkeletalMeshComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
	const FPrimitiveSceneProxy* Proxy = GetSceneProxy();
	if (!SkeletalMesh || !Proxy)
	{
		return;
	}

	const FMeshRenderState* State = Proxy->GetRenderState();
	if (!State)
	{
		return;
	}

	const int32 LODIndex = 0;

	const FCachedMeshLOD& LOD = State->LODs[LODIndex];
	for (uint32 I = 0; I < LOD.NumSections; ++I)
	{
		const FCachedMeshSection& Section = State->Sections[LOD.FirstSection + I];

		FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
		Packet.Proxy = Proxy;
		Packet.Geometry.VertexBuffers[0] = LOD.VertexBuffers[0];
		Packet.Geometry.VertexBuffers[1] = LOD.VertexBuffers[1];
		Packet.Geometry.VertexBufferCount = 2;
		Packet.Geometry.IndexBuffer = LOD.IndexBuffer;
		Packet.Geometry.StartIndex = Section.StartIndex;
		Packet.Geometry.IndexCount = Section.IndexCount;
		Packet.Material = Section.Material;
	}
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	WorldRootMotionMatrix = FMatrix::Identity;
	AActor* Owner = GetOwner();
	UCharacterMovementComponent* Movement = Owner
		? Cast<UCharacterMovementComponent>(Owner->FindComponentByClass(UCharacterMovementComponent::StaticClass()))
		: nullptr;

	if (!SkeletalMesh || !AnimInstance || ReferencePose.IsEmpty())
	{
		if (Movement) Movement->ApplyRootMotion(WorldRootMotionMatrix, false);
		return;
	}

	PoseContext Context{ ReferencePose, ReferencePose };

	if (Movement)
	{
		const FVector& Velocity = Movement->Velocity;
		AnimInstance->SetSpeed(std::sqrt(Velocity.X * Velocity.X + Velocity.Y * Velocity.Y));
	}

	AnimInstance->Tick(DeltaTime, Context);
	ApplyLocalPose(Context.LocalPoseTrans);
	UpdateWorldRootMotion();
	if (Movement)
	{
		const bool bRootMotionActive = NodeGlobalTransforms.IsValidIndex(AnimInstance->GetRootMotionNodeIndex());
		Movement->ApplyRootMotion(WorldRootMotionMatrix, bRootMotionActive);
	}

	if (AnimInstance->PendingNotifyNames.Num()!=0)
	{
		for (int i = 0;i < AnimInstance->PendingNotifyNames.Num();i++)
		{
			GetOwner()->HandleAnimNotify(AnimInstance->PendingNotifyNames[i]);
		}
	}
}

void USkeletalMeshComponent::UpdateWorldRootMotion()
{
	const int32 RootNodeIndex = AnimInstance->GetRootMotionNodeIndex();
	if (!NodeGlobalTransforms.IsValidIndex(RootNodeIndex))
		return;

	// Root Lock된 루트 로컬 * 부모 누적 변환 * 메시 월드 변환.
	const FMatrix RootWorldMatrix = NodeGlobalTransforms[RootNodeIndex] * GetWorldMatrix();
	const float Determinant = RootWorldMatrix.Determinant();
	if (!std::isfinite(Determinant) || std::abs(Determinant) <= SMALL_NUMBER)
		return;

	const FMatrix LocalDelta = AnimInstance->GetRootTransform().GetLocalMatrix();
	// 행 벡터 규약: 루트 기준 변화량을 월드 기준 변화량으로 변환한다.
	WorldRootMotionMatrix = RootWorldMatrix.Inverse() * LocalDelta * RootWorldMatrix;
}

void USkeletalMeshComponent::ApplyLocalPose(const TArray<PoseTransform>& InPose)
{
	if (InPose.Num() != ReferencePose.Num())
		return;
	LocalPose.SetNum(InPose.Num());
	for (int i = 0;i < InPose.Num();i++)
	{
		const PoseTransform& Transform = InPose[i];
		FMatrix Matrix = Transform.Rotation.ToFMatrix();
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			Matrix[0][Axis] *= Transform.Scale.X;
			Matrix[1][Axis] *= Transform.Scale.Y;
			Matrix[2][Axis] *= Transform.Scale.Z;
		}
		Matrix.SetOrigin(Transform.Translation);
		LocalPose[i] = Matrix;
	}
	UpdateBoneMatrices();
}
