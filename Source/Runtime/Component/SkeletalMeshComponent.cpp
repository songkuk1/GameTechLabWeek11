#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
	delete AnimInstance;
}

void USkeletalMeshComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Ar = Record.GetUnderlyingArchive();
	if (Ar.HasAnyPortFlags(EPropertyPortFlags::PPF_DuplicateForPIE))
	{
		// 에셋은 공유하고, 재생 상태는 Play 컴포넌트에 새로 만든다.
		UObject* Sequence = GetAnimationSequence();
		Record << SA_VALUE("AnimationSequence", Sequence);
		bool bLoop = IsAnimationLooping();
		Record << SA_VALUE("AnimationLooping", bLoop);
		if (Ar.IsLoading())
		{
			SetAnimationSequence(Cast<UAnimSequence>(Sequence));
			SetAnimationLooping(bLoop);
		}
	}
}

UAnimSequence* USkeletalMeshComponent::GetAnimationSequence() const
{
	return AnimInstance ? AnimInstance->GetSequence() : nullptr;
}

void USkeletalMeshComponent::SetAnimationLooping(bool bLoop)
{
	if (!AnimInstance) AnimInstance = NewObject<UAnimationInstance>(this);
	AnimInstance->SetLoop(bLoop);
}

void USkeletalMeshComponent::SetAnimationSequence(UAnimSequence* Sequence)
{
	if (Sequence)
	{
		if (!SkeletalMesh) return;
		const FString Prefix = SkeletalMesh->GetPath() + "_Anim_";
		if (Sequence->GetPath().compare(0, Prefix.size(), Prefix) != 0) return;
	}
	if (!AnimInstance) AnimInstance = NewObject<UAnimationInstance>(this);
	AnimInstance->SetSequence(Sequence);
	AnimInstance->SetCurrentPlayTime(0.0f);
	ResetPose();
	if (Sequence) Sequence->CalculateCurrentPose(0.0f, LocalPose);
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

	if (!SkeletalMesh || !AnimInstance || !AnimInstance->GetSequence())
		return;

	AnimInstance->Tick(DeltaTime, LocalPose);
	UpdateBoneMatrices();
}
