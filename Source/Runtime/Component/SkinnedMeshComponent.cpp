#include "EnginePCH.h"
#include "Component/SkinnedMeshComponent.h"

#include "Animation/Skeleton.h"
#include "Render/LineBatcher.h"

#include <algorithm>

namespace
{
	// 관절 위치에 서로 수직인 원 세 개로 와이어 구를 그린다
	void DrawJointSphere(FLineBatcher* LineBatcher, const FVector& Center, const float Radius, const FVector4& Color)
	{
		constexpr int32 SegmentCount = 12;
		const FVector PlaneAxes[3][2] =
		{
			{ FVector::XAxisVector, FVector(0.0f, 1.0f, 0.0f) },
			{ FVector::XAxisVector, FVector(0.0f, 0.0f, 1.0f) },
			{ FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) },
		};

		for (const auto& Axes : PlaneAxes)
		{
			FVector Prev = Center + Axes[0] * Radius;
			for (int32 i = 1; i <= SegmentCount; i++)
			{
				const float Angle = 2.0f * PI * i / SegmentCount;
				const FVector Next = Center + (Axes[0] * std::cos(Angle) + Axes[1] * std::sin(Angle)) * Radius;
				LineBatcher->AddLine(Prev, Next, Color);
				Prev = Next;
			}
		}
	}

	// 언리얼 에디터의 본처럼 부모 관절에서 자식 관절로 뻗는 팔면체를 그린다
	void DrawBoneShape(FLineBatcher* LineBatcher, const FVector& Start, const FVector& End, const float MaxRadius, const FVector4& Color)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Size();
		if (Length < SMALL_NUMBER)
			return;

		// 뼈 방향과 가장 덜 평행한 축으로 단면의 두 축을 만든다
		const FVector Dir = Delta / Length;
		const FVector Helper = std::abs(Dir.Z) < 0.9f ? FVector(0.0f, 0.0f, 1.0f) : FVector::XAxisVector;
		const FVector U = Dir.Cross(Helper).GetSafeNormal();
		const FVector V = Dir.Cross(U);

		// 단면은 부모 쪽 15% 지점에 둔다. 짧은 뼈는 두께도 같이 줄인다
		const float Radius = std::min(Length * 0.1f, MaxRadius);
		const FVector Base = Start + Dir * (Length * 0.15f);
		const FVector Corners[4] = { Base + U * Radius, Base + V * Radius, Base - U * Radius, Base - V * Radius };

		for (int32 i = 0; i < 4; i++)
		{
			LineBatcher->AddLine(Start, Corners[i], Color);
			LineBatcher->AddLine(Corners[i], End, Color);
			LineBatcher->AddLine(Corners[i], Corners[(i + 1) % 4], Color);
		}
	}
}

USkinnedMeshComponent::~USkinnedMeshComponent() = default;

void USkinnedMeshComponent::PostLoad()
{
	Super::PostLoad();

	UpdateBoneMatrices();
}

void USkinnedMeshComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	UpdateBoneMatrices();
}

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	SkeletalMesh = InMesh;

	ClearOverrideMaterials();

	if (SkeletalMesh)
	{
		const auto& Materials = SkeletalMesh->GetMaterials();
		for (int32 i = 0; i < Materials.Num(); i++)
		{
			SetMaterial(i, Materials[i]);
		}
	}

	ResetPose();
	UpdateBoneMatrices();
	MarkRenderStateDirty();
	OnTransformDirty();
}

void USkinnedMeshComponent::ResetPose()
{
	LocalPose.Reset();
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton)
		return;

	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
	LocalPose.SetNum(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); i++)
		LocalPose[i] = Nodes[i].LocalTransform;
}

void USkinnedMeshComponent::SetBoneComponentMatrix(const int32 BoneIndex, const FMatrix& ComponentMatrix)
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton)
		return;
	const TArray<FModelBone>& Bones = Skeleton->GetBones();
	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
	if (BoneIndex < 0 || BoneIndex >= Bones.Num() || LocalPose.Num() != Nodes.Num())
		return;

	// 행 벡터 규약: Global = Local × Global(부모) 이므로 Local = Global × Global(부모)⁻¹
	const int32 NodeIndex = Bones[BoneIndex].NodeIndex;
	const int32 ParentNodeIndex = Nodes[NodeIndex].ParentNodeIndex;
	const FMatrix ParentGlobal = ParentNodeIndex != -1 ? NodeGlobalTransforms[ParentNodeIndex] : FMatrix::Identity;
	LocalPose[NodeIndex] = ComponentMatrix * ParentGlobal.Inverse();

	// 자식 본도 이 본을 따라가도록 계층 전체를 다시 누적한다
	UpdateBoneMatrices();
}

void USkinnedMeshComponent::UpdateBoneMatrices()
{
	if (!SkeletalMesh)
	{
		return;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		return;
	}

	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
	const TArray<FModelBone>& Bones = Skeleton->GetBones();

	// 로드 직후처럼 포즈가 아직 없으면 에셋 기본 자세로 시작한다
	if (LocalPose.Num() != Nodes.Num())
		ResetPose();

	NodeGlobalTransforms.SetNum(Nodes.Num());          // 지역 GlobalTransforms 대신 멤버 사용
	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		const FModelNode& Node = Nodes[i];
		FMatrix ParentMatrix = FMatrix::Identity;
		if (Node.ParentNodeIndex != -1)
			ParentMatrix = NodeGlobalTransforms[Node.ParentNodeIndex];
		NodeGlobalTransforms[i] = LocalPose[i] * ParentMatrix;
	}

	BoneMatrices.SetNum(Bones.Num());
	for (int32 i = 0; i < Bones.Num(); i++)
	{
		const FModelBone& Bone = Bones[i];

		//그 본이 바인드 포즈에서 지금까지 얼마나 움직였는가
		BoneMatrices[i] = Bone.InverseBindTransform * NodeGlobalTransforms[Bone.NodeIndex];
	}

	MarkRenderStateDirty();
}

int32 USkinnedMeshComponent::GetNumMaterials() const
{
	return SkeletalMesh ? SkeletalMesh->GetMaterials().Num() : 0;
}

FString USkinnedMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
	return SkeletalMesh ? SkeletalMesh->GetMeshData().MaterialSlots[SlotIndex].Name : FString();
}

UMaterial* USkinnedMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
	return SkeletalMesh ? SkeletalMesh->GetMaterials()[SlotIndex] : nullptr;
}

const TArray<FMatrix>& USkinnedMeshComponent::GetBoneMatrices() const
{
	return BoneMatrices;
}

const TArray<FMatrix>& USkinnedMeshComponent::GetLocalPose() const
{
	return LocalPose;
}

USkeletalMesh* USkinnedMeshComponent::GetSkeletalMesh()
{
	return SkeletalMesh;
}
FMatrix USkinnedMeshComponent::GetBoneComponentMatrix(const int32 BoneIndex) const
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!Skeleton) return FMatrix::Identity;
	const TArray<FModelBone>& Bones = Skeleton->GetBones();
	if (BoneIndex < 0 || BoneIndex >= Bones.Num()) return FMatrix::Identity;
	return NodeGlobalTransforms[Bones[BoneIndex].NodeIndex];
}

void USkinnedMeshComponent::DrawDebugSkeleton(FLineBatcher* LineBatcher, const int32 SelectedBone) const
{
	USkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
	if (!LineBatcher || !Skeleton)
		return;
	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();
	const TArray<FModelBone>& Bones = Skeleton->GetBones();
	if (NodeGlobalTransforms.Num() != Nodes.Num())
		return;

	// 노드마다 본 번호를 매겨, 부모가 본이 아닌 노드(씬 루트·메시 노드)로는 선을 긋지 않는다
	TArray<int32> NodeToBone;
	NodeToBone.SetNum(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); i++)
		NodeToBone[i] = -1;
	for (int32 i = 0; i < Bones.Num(); i++)
		NodeToBone[Bones[i].NodeIndex] = i;

	// 노드 행렬은 컴포넌트 기준이다. 행 벡터 규약이라 컴포넌트 월드 행렬을 오른쪽에 곱한다
	const FMatrix ComponentWorld = GetWorldMatrix();
	auto JointWorld = [&](const int32 NodeIndex) { return NodeGlobalTransforms[NodeIndex] * ComponentWorld; };

	// 관절·뼈 두께는 메시 크기에 맞춘다. 단위가 cm든 m든 비슷한 비율로 보인다
	const FBox Bounds = SkeletalMesh->GetMeshData().AABB;
	const FVector Extent = Bounds.Max - Bounds.Min;
	const float MeshSize = std::max({ Extent.X, Extent.Y, Extent.Z });
	const float JointRadius = MeshSize * 0.006f;
	const float BoneRadius = MeshSize * 0.02f;

	const FVector4 BoneColor(0.85f, 0.85f, 0.85f, 1.0f);
	const FVector4 SelectedColor(1.0f, 0.85f, 0.2f, 1.0f);
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); BoneIndex++)
	{
		const int32 NodeIndex = Bones[BoneIndex].NodeIndex;
		const FMatrix World = JointWorld(NodeIndex);
		const FVector Joint(World.M[3][0], World.M[3][1], World.M[3][2]);
		const FVector4& Color = BoneIndex == SelectedBone ? SelectedColor : BoneColor;

		const int32 ParentNodeIndex = Nodes[NodeIndex].ParentNodeIndex;
		if (ParentNodeIndex != -1 && NodeToBone[ParentNodeIndex] != -1)
		{
			const FMatrix ParentWorld = JointWorld(ParentNodeIndex);
			const FVector ParentJoint(ParentWorld.M[3][0], ParentWorld.M[3][1], ParentWorld.M[3][2]);
			DrawBoneShape(LineBatcher, ParentJoint, Joint, BoneRadius, Color);
		}

		DrawJointSphere(LineBatcher, Joint, JointRadius, Color);
	}
}