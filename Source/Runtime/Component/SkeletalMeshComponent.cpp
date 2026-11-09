#include "EnginePCH.h"
#include "Component/SkeletalMeshComponent.h"

#include "UObject/UObjectGlobals.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Render/RenderPacket.h"
#include "GameFramework/Actor.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationGraph.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = ETickingGroup::PostPhysics;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    TickAnimation(DeltaTime);
}

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

    USkeleton* ComponentSkeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;

    if (!ComponentSkeleton || InSequence->GetSkeleton() != ComponentSkeleton)
    {
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

    USkeleton* ComponentSkeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;

    if (!ComponentSkeleton || InSequenceA->GetSkeleton() != ComponentSkeleton || InSequenceB->GetSkeleton() != ComponentSkeleton)
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

    USkeleton* ComponentSkeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;

    if (!ComponentSkeleton || InBlendSpace->GetSkeleton() != ComponentSkeleton)
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

    FVector RootMotionTranslation;
    FQuat RootMotionRotation;

    if (AnimInstance->ConsumeRootMotionDelta(RootMotionTranslation, RootMotionRotation))
    {
        if (AActor* Owner = GetOwner())
        {
            const FQuat ActorRotation = Owner->GetActorQuat();
            const FVector NewLocation = Owner->GetActorLocation() + ActorRotation.RotateVector(RootMotionTranslation);
            const FQuat NewRotation = (ActorRotation * RootMotionRotation).Normalized();

            Owner->SetActorTransform(FTransform(NewLocation, Owner->GetActorScale3D(), NewRotation.ToFRotator()));
        }
    }

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
        Packet.Geometry.SetVertexBuffers(LOD.VertexBuffers, LOD.VertexBufferCount);
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

UAnimInstance* USkeletalMeshComponent::GetOrCreateAnimInstance()
{
    return EnsureAnimInstance() ? AnimInstance : nullptr;
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

void USkeletalMeshComponent::SetAnimationRootNode(FAnimNode_Base* InRootNode)
{
    if (!InRootNode)
    {
        if (AnimInstance)
        {
            AnimInstance->SetRootNode(nullptr);
        }
        bPlayingAnimation = false;
        ResetPose();
        UpdateBoneMatrices();
        return;
    }
    if (!EnsureAnimInstance())
    {
        return;
    }
    if (!AnimInstance->SetRootNode(InRootNode))
    {
        return;
    }

    bPlayingAnimation = true;
    TickAnimation(0.0f);
}

bool USkeletalMeshComponent::SetAnimationGraph(
    UAnimationGraph* InGraph)
{
    if (!InGraph || !SkeletalMesh || InGraph->GetSkeleton() != SkeletalMesh->GetSkeleton())
    {
        return false;
    }

    if (!EnsureAnimInstance())
    {
        return false;
    }

    if (!AnimInstance->InitializeAnimationGraph(InGraph))
    {
        return false;
    }

    bPlayingAnimation = true;
    TickAnimation(0.0f);
    return true;
}

void USkeletalMeshComponent::HandleAnimNotify(const FName& NotifyName, EAnimNotifyEventType EventType, float DeltaSeconds)
{
    AActor* Owner = GetOwner();

    if (!Owner || NotifyName.IsNone())
    {
        return;
    }

    if (EventType == EAnimNotifyEventType::Trigger)
    {
        Owner->HandleAnimNotify(NotifyName);
    }
    else
    {
        Owner->HandleAnimNotifyState(NotifyName, EventType, DeltaSeconds);
    }
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