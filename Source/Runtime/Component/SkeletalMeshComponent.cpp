#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

#include "UObject/UObjectGlobals.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    delete AnimInstance;
    AnimInstance = nullptr;
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeAnimationInstance() const
{
    return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

UAnimTwoWayBlendInstance* USkeletalMeshComponent::GetTwoWayBlendAnimationInstance() const
{
    return Cast<UAnimTwoWayBlendInstance>(AnimInstance);
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* InSequence)
{
    if (!InSequence)
    {
        bPlayingAnimation = false;

        if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
        {
            SingleNode->SetAnimation(nullptr);
        }

        ResetPose();
        UpdateBoneMatrices();
        return;
    }

    if (!GetSingleNodeAnimationInstance())
    {
        delete AnimInstance;
        AnimInstance = nullptr;

        AnimInstance = NewObject<UAnimSingleNodeInstance>(this);

        if (!AnimInstance)
        {
            return;
        }

        AnimInstance->InitializeAnimation(this);
    }

    UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance();

    if (!SingleNode)
    {
        return;
    }

    SingleNode->SetAnimation(InSequence);

    bPlayingAnimation = true;

    TickAnimation(0.0f);
}

void USkeletalMeshComponent::PlayBlendAnimation(UAnimSequence* InSequenceA, UAnimSequence* InSequenceB, float InAlpha)
{
    if (!InSequenceA || !InSequenceB)
    {
        return;
    }

    if (!GetTwoWayBlendAnimationInstance())
    {
        delete AnimInstance;
        AnimInstance = nullptr;
        AnimInstance = NewObject<UAnimTwoWayBlendInstance>(this);
        if (!AnimInstance)
        {
            return;
        }
        AnimInstance->InitializeAnimation(this);
    }
    UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance();
    if (!BlendInstance)
    {
        return;
    }
    BlendInstance->SetAnimations(InSequenceA, InSequenceB);
    BlendInstance->SetAlpha(InAlpha);
    bPlayingAnimation = true;
    TickAnimation(0.0f);
}

void USkeletalMeshComponent::SetAnimationBlendAlpha(float InAlpha)
{
    UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance();
    if (!BlendInstance)
    {
        return;
    }
    BlendInstance->SetAlpha(InAlpha);
    EvaluateAnimationPose();
}

float USkeletalMeshComponent::GetAnimationBlendAlpha() const
{
    const UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance();
    return BlendInstance ? BlendInstance->GetAlpha() : 0.0f;
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
    if (!bPlayingAnimation)
    {
        return;
    }

    if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        SingleNode->SetPlaybackPosition(InTime);
    }
    else if (UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        BlendInstance->SetPlaybackPosition(InTime);
    }
    else
    {
        return;
    }

    EvaluateAnimationPose();
}

float USkeletalMeshComponent::GetAnimationPlaybackTime() const
{
    if (const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        return SingleNode->GetCurrentTime();
    }

    if (const UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        return BlendInstance->GetCurrentTime();
    }

    return 0.0f;
}

float USkeletalMeshComponent::GetAnimationPlayLength() const
{
    if (const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        return SingleNode->GetPlayLength();
    }

    if (const UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        return BlendInstance->GetPlayLength();
    }

    return 0.0f;
}

float USkeletalMeshComponent::GetAnimationFrameRate() const
{
    if (const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        return SingleNode->GetFrameRate();
    }

    if (const UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        return BlendInstance->GetFrameRate();
    }

    return 0.0f;
}

void USkeletalMeshComponent::SetAnimationPlayRate(float InPlayRate)
{
    if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        SingleNode->SetPlayRate(InPlayRate);
    }
    else if (UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        BlendInstance->SetPlayRate(InPlayRate);
    }
}

float USkeletalMeshComponent::GetAnimationPlayRate() const
{
    if (const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        return SingleNode->GetPlayRate();
    }

    if (const UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        return BlendInstance->GetPlayRate();
    }

    return 1.0f;
}

void USkeletalMeshComponent::SetAnimationLooping(bool bInLoop)
{
    if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        SingleNode->SetLooping(bInLoop);
    }
    else if (UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        BlendInstance->SetLooping(bInLoop);
    }
}

bool USkeletalMeshComponent::IsAnimationLooping() const
{
    if (const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        return SingleNode->IsLooping();
    }

    if (const UAnimTwoWayBlendInstance* BlendInstance = GetTwoWayBlendAnimationInstance())
    {
        return BlendInstance->IsLooping();
    }

    return true;
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