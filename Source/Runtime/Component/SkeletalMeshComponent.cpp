#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
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
	if (!SkeletalMesh || !AnimInstance || ReferencePose.IsEmpty())
		return;

	PoseContext Context{ ReferencePose, ReferencePose };

	AnimInstance->Tick(DeltaTime, Context);
	if (Context.LocalPoseTrans.Num() != ReferencePose.Num())
		return;
	LocalPose.SetNum(Context.LocalPoseTrans.Num());
	for (int i = 0;i < Context.LocalPoseTrans.Num();i++)
	{
		const PoseTransform& Transform = Context.LocalPoseTrans[i];
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
