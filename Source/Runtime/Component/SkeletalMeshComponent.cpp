#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

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

    if (!AnimInstance)
    {
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

    UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance();

    if (!SingleNode)
    {
        return;
    }

    SingleNode->SetPlaybackPosition(InTime);

    EvaluateAnimationPose();
}

float USkeletalMeshComponent::GetAnimationPlaybackTime() const
{
    const UAnimSingleNodeInstance* SingleNode =  GetSingleNodeAnimationInstance();

    return SingleNode ? SingleNode->GetCurrentTime() : 0.0f;
}

float USkeletalMeshComponent::GetAnimationPlayLength() const
{
    const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance();

    return SingleNode ? SingleNode->GetPlayLength() : 0.0f;
}

float USkeletalMeshComponent::GetAnimationFrameRate() const
{
    const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance();

    return SingleNode ? SingleNode->GetFrameRate() : 0.0f;
}

void USkeletalMeshComponent::SetAnimationPlayRate(float InPlayRate)
{
    if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        SingleNode->SetPlayRate(InPlayRate);
    }
}

float USkeletalMeshComponent::GetAnimationPlayRate() const
{
    const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance();

    return SingleNode ? SingleNode->GetPlayRate() : 1.0f;
}

void USkeletalMeshComponent::SetAnimationLooping(bool bInLoop)
{
    if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance())
    {
        SingleNode->SetLooping(bInLoop);
    }
}

bool USkeletalMeshComponent::IsAnimationLooping() const
{
    const UAnimSingleNodeInstance* SingleNode = GetSingleNodeAnimationInstance();

    return SingleNode ? SingleNode->IsLooping() : true;
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