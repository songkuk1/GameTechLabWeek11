#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

#include "UObject/UObjectGlobals.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace1D.h"
USkeletalMeshComponent::~USkeletalMeshComponent()
{
    delete AnimInstance;
    AnimInstance = nullptr;
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* InSequence)
{
    if (!InSequence)
    {
        bPlayingAnimation = false;
        if (AnimInstance)
        {
            AnimInstance->PlayAnimation(nullptr);
        }
        ResetPose();
        UpdateBoneMatrices();
        return;
    }

    if (!EnsureAnimInstance())
    {
        return;
    }

    AnimInstance->PlayAnimation(InSequence);
    bPlayingAnimation = true;
    TickAnimation(0.0f);
}

void USkeletalMeshComponent::PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha)
{
    if (!InSequenceA || !InSequenceB)
    {
        return;
    }

    if (!EnsureAnimInstance())
    {
        return;
    }

    AnimInstance->PlayBlendAnimation(InSequenceA, InSequenceB, InAlpha);

    bPlayingAnimation = true;
    TickAnimation(0.0f);
}

void USkeletalMeshComponent::SetAnimationBlendAlpha(float InAlpha)
{
    if (!AnimInstance)
    {
        return;
    }

    AnimInstance->SetAnimationBlendAlpha(InAlpha);
    EvaluateAnimationPose();
}

float USkeletalMeshComponent::GetAnimationBlendAlpha() const
{
    return AnimInstance ? AnimInstance->GetAnimationBlendAlpha() : 0.0f;
}

void USkeletalMeshComponent::PlayBlendSpace(UBlendSpace1D* InBlendSpace, float InInput)
{
    if (!InBlendSpace || InBlendSpace->GetSamples().IsEmpty())
    {
        return;
    }

    if (!EnsureAnimInstance())
    {
        return;
    }

    AnimInstance->PlayBlendSpace(InBlendSpace, InInput);
    bPlayingAnimation = true;
    TickAnimation(0.0f);
}

void USkeletalMeshComponent::SetAnimationBlendInput(float InInput)
{
    if (!AnimInstance)
    {
        return;
    }
    AnimInstance->SetAnimationBlendInput(InInput);
    AnimInstance->UpdateAnimation(0.0f);
    EvaluateAnimationPose();
}

float USkeletalMeshComponent::GetAnimationBlendInput() const
{
    return AnimInstance ? AnimInstance->GetAnimationBlendInput() : 0.0f;
}

void USkeletalMeshComponent::TickAnimation(float DeltaSeconds)
{
    if (!bPlayingAnimation || !AnimInstance)
    {
        return;
    }

    AnimInstance->UpdateAnimation(DeltaSeconds);

    EvaluateAnimationPose();
}
 
void USkeletalMeshComponent::SetAnimationPlaybackTime(float InTime)
{
    if (!bPlayingAnimation || !AnimInstance)
    {
        return;
    }
    AnimInstance->SetPlaybackPosition(InTime);
    EvaluateAnimationPose();
}

float USkeletalMeshComponent::GetAnimationPlaybackTime() const
{
    return AnimInstance ? AnimInstance->GetCurrentTime() : 0.0f;
}

float USkeletalMeshComponent::GetAnimationPlayLength() const
{
    return AnimInstance ? AnimInstance->GetPlayLength() : 0.0f;
}

float USkeletalMeshComponent::GetAnimationFrameRate() const
{
    return AnimInstance ? AnimInstance->GetFrameRate() : 0.0f;
}

void USkeletalMeshComponent::SetAnimationPlayRate(float InPlayRate)
{
    if (AnimInstance)
    {
        AnimInstance->SetPlayRate(InPlayRate);
    }
}

float USkeletalMeshComponent::GetAnimationPlayRate() const
{
    return AnimInstance ? AnimInstance->GetPlayRate() : 1.0f;
}

void USkeletalMeshComponent::SetAnimationLooping(bool bInLoop)
{
    if (AnimInstance)
    {
        AnimInstance->SetLooping(bInLoop);
    }
}

bool USkeletalMeshComponent::IsAnimationLooping() const
{
    return AnimInstance ? AnimInstance->IsLooping() : true;
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

void USkeletalMeshComponent::SetAnimInstanceClass(UClass* InAnimInstanceClass)
{
    if (!InAnimInstanceClass || !InAnimInstanceClass->IsChildOf(UAnimInstance::StaticClass()))
    {
        return;
    }
    if (AnimInstance && AnimInstance->GetClass() == InAnimInstanceClass)
    {
        return;
    }
    delete AnimInstance;
    AnimInstance = nullptr;
    bPlayingAnimation = false;
    AnimInstance = NewObject<UAnimInstance>(this, InAnimInstanceClass);
    if (!AnimInstance)
    {
        return;
    }
    AnimInstance->InitializeAnimation(this);
    bPlayingAnimation = true;
    EvaluateAnimationPose();
}

bool USkeletalMeshComponent::EnsureAnimInstance()
{
    if (AnimInstance)
    {
        return true;
    }

    AnimInstance = NewObject<UAnimInstance>(this);

    if (!AnimInstance)
    {
        return false;
    }

    AnimInstance->InitializeAnimation(this);
    return true;
}

void USkeletalMeshComponent::EvaluateAnimationPose()
{
    if (!bPlayingAnimation || !SkeletalMesh || !AnimInstance)
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

    AnimInstance->EvaluateAnimation(Output);

    LocalPose = Output.Pose;

    UpdateBoneMatrices();
}