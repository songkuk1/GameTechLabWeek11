#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"
#include "Animation/Skeleton.h"
void USkeletalMeshComponent::PlayAnimation(UAnimSequence* InSequence)
{
	AnimationPlayer.SetSequence(InSequence);
	bPlayingAnimation = (InSequence != nullptr);
	if (!bPlayingAnimation)
	{
		ResetPose();
		UpdateBoneMatrices();
		return;
	}
	TickAnimation(0.0f);
}

void USkeletalMeshComponent::TickAnimation(float DeltaSeconds)
{
	if (!bPlayingAnimation)
	{
		return;
	}
	FAnimationUpdateContext Context;
	Context.DeltaTime = DeltaSeconds;
	AnimationPlayer.Update(Context);
	EvaluateAnimationPose();
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

void USkeletalMeshComponent::SetAnimationPlaybackTime(float InTime)
{
	if (!bPlayingAnimation)
	{
		return;
	}
	AnimationPlayer.SetPlaybackPosition(InTime);
	EvaluateAnimationPose();
}

void USkeletalMeshComponent::EvaluateAnimationPose()
{
	if (!bPlayingAnimation || !SkeletalMesh)
	{
		return;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

	if (!Skeleton)
	{
		return;
	}

	const TArray<FModelNode>& Nodes = Skeleton->GetNodes();

	if (Nodes.IsEmpty())
	{
		return;
	}

	FPoseContext Output;
	Output.Pose.SetNum(Nodes.Num());

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		Output.Pose[Index] = Nodes[Index].LocalTransform;
	}

	AnimationPlayer.Evaluate(Output);

	LocalPose = Output.Pose;

	UpdateBoneMatrices();
}
