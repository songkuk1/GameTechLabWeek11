#include "EnginePCH.h"

#include "Editor/Rendering/ViewportRenderCollector.h"
#include "Editor/Viewports/Viewport.h"

#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/ParticleSubUVComponent.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/World.h"

#include <algorithm>

// 현재 World의 가시 컴포넌트에서 경계만 캡처한다. Mesh·삼각형은 복사하지 않는다.
void FViewportRenderCollector::CaptureWorld(UWorld& World, bool bDrawSkeletalMeshes)
{
    RenderObjects.Reset();
    for (auto& Entry : PrimitiveById) Entry.second.bCaptured = false;
    bCapturedBillboard = false;
    bCapturedParticle = false;

    const FScene& Scene = World.GetScene();
    const int32 Count = Scene.Proxies.Num();
    for (int32 i = 0; i < Count;++i)
    {
        if (!Scene.PrimitiveFlags[i]) continue;

        // 스켈레탈 표시 설정에 따른 렌더 후보 제외
        if (!bDrawSkeletalMeshes && Scene.Proxies[i]->IsUsingSkinning())
        {
            continue;
        }
        UPrimitiveComponent* Primitive = Scene.Proxies[i]->GetComponent();
        if (!Primitive || !Primitive->IsVisible() || !Primitive->GetOwner() ||
            Primitive->GetOwner()->GetWorld() != &World) continue;
        if (World.IsPlayInEditor() && Primitive->IsHiddenInGame()) continue;
        const ObjectId Id = Primitive->GetUUID();
        if (Id == InvalidObjectId) continue;

        FRenderableObject RenderObject{};
        RenderObject.Id = Id;
        const FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy();
        RenderObject.WorldBounds = Proxy ? Proxy->GetBounds() : MakeWorldBounds(Primitive->CalcBounds());
        RenderObjects.Add(RenderObject);
        PrimitiveSnapshot& Snapshot = PrimitiveById[Id];
        Snapshot.Primitive = Primitive;
        Snapshot.bCaptured = true;
        Snapshot.bParticlesPrepared = false;
        if (Cast<UParticleSubUVComponent>(Primitive)) bCapturedParticle = true;
        else if (Cast<UBillboardComponent>(Primitive)) bCapturedBillboard = true;
    }

    for (auto Iterator = PrimitiveById.begin(); Iterator != PrimitiveById.end();)
    {
        const auto Current = Iterator++;
        if (!Current->second.bCaptured) PrimitiveById.Remove(Current->first);
    }
}

// 가시 ID를 컴포넌트로 역매핑해 큐를 구성한다. 생존 목록·상수는 프레임 공통, Billboard·정렬 거리는 View별이다.
void FViewportRenderCollector::BuildRenderQueue(const FViewport& Viewport, FRenderQueue& OutQueue)
{
    OutQueue.Reset();
    PendingStaticMeshes.Reset();
    LODInputs.Reset();
    const FViewport::FPreparedView& Prepared = Viewport.PrepareView();
    {
        CullForView(RenderObjects, Prepared.Frustum, VisibleIds);
    }
    const FRect Rect = Viewport.GetOutputRect();
    const FViewCamera& ViewCamera = Viewport.GetCamera();
    const FMatrix Projection = BuildProjectionMatrix(
        ViewCamera.Projection, Rect.Width / Rect.Height);
    const float ScaleX = Projection.M[1][0];
    const float ScaleY = Projection.M[2][1];

    FLODViewContext LODContext{
        static_cast<uint32>(Rect.Width),
        static_cast<uint32>(Rect.Height)
    };
    LODContext.CameraPosition = Viewport.GetCameraLocation();
    LODContext.ViewProjection = Prepared.ViewProjection;
    LODContext.CameraForward = Viewport.GetCameraForward();
    LODContext.ProjectionScaleSquared = std::max(ScaleX * ScaleX, ScaleY * ScaleY);
    LODContext.NearZ = ViewCamera.Projection.NearClip;
    LODContext.bOrthographic = ViewCamera.Projection.Mode == EProjectionMode::Orthographic;
    LODContext.Prepare();

    for (const ObjectId Id : VisibleIds)
    {
        const auto Found = PrimitiveById.Find(Id);
        if (!Found || !Found->Primitive)
            continue;

        UPrimitiveComponent* Primitive = Found->Primitive;
        if (UParticleSubUVComponent* ParticleComponent = Cast<UParticleSubUVComponent>(Primitive))
        {
            const TArray<FParticle>& Particles = ParticleComponent->GetParticlesForView();
            PrimitiveSnapshot& Snapshot = *Found;
            if (!Snapshot.bParticlesPrepared)
            {
                Snapshot.AliveParticleIndices.Reset();
                Snapshot.AliveParticleIndices.Reserve(Particles.Num());
                for (int32 Index = 0; Index < Particles.Num(); ++Index)
                    if (Particles[Index].bAlive) Snapshot.AliveParticleIndices.Add(Index);
                ParticleComponent->BeginViewSubmission();
                Snapshot.bParticlesPrepared = true;
            }
            // 반투명은 모든 emitter를 합친 최종 Renderer만 정렬한다. 불투명은 Core 정렬을 유지하며 동률은 인덱스 순서다.
            const FVector CameraLocation = Viewport.GetCameraLocation();
            const bool bOpaque = ParticleComponent->UsesOpaqueMaterial();
            if (bOpaque)
            {
                SortInputs.Reset();
                for (const int32 Index : Snapshot.AliveParticleIndices)
                    SortInputs.Add({static_cast<ObjectId>(Index + 1), Particles[Index].Location});
                SortParticlesByCameraDistance(SortInputs, ViewCamera.Transform.Location, SortedParticleIds);
            }
            for (int32 Order = 0; Order < Snapshot.AliveParticleIndices.Num(); ++Order)
            {
                const int32 ParticleIndex = bOpaque ? static_cast<int32>(SortedParticleIds[Order] - 1) : Snapshot.AliveParticleIndices[Order];
                const FParticle& Particle = Particles[ParticleIndex];
                const FMatrix ParticleWorld = Viewport.BuildBillboardMatrix(Particle.Location, Particle.Scale, Particle.Scale);
                const FVector Delta = Particle.Location - CameraLocation;
                ParticleComponent->SubmitParticleToRenderQueue(OutQueue, ParticleIndex, ParticleWorld, Delta.Dot(Delta));
            }
        }
        else if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Primitive))
        {
            const FVector Scale = Billboard->GetWorldScale3D();
            Billboard->SubmitToRenderQueue(
                OutQueue,
                Viewport.BuildBillboardMatrix(Billboard->GetWorldLocation(), Scale.Y, Scale.Z));
        }
        else if (auto* StaticComponent = Cast<UStaticMeshComponent>(Primitive))
        {
            UStaticMesh* Mesh = StaticComponent->GetStaticMesh();
            if (!Mesh)
                continue;

            const FPrimitiveSceneProxy* Proxy = StaticComponent->GetSceneProxy();
            if (Proxy)
            {
                PendingStaticMeshes.Add(StaticComponent);
                LODInputs.Add({Proxy->GetLODSphere(), Proxy->GetRenderState()});
            }
        }
        else
        {
            Primitive->SubmitToRenderQueue(OutQueue);
        }
    }
    SelectLODs(LODInputs, LODContext, SelectedLODs);
    for (uint32 I = 0; I < static_cast<uint32>(PendingStaticMeshes.Num()); ++I)
        PendingStaticMeshes[I]->SubmitToRenderQueue(OutQueue, SelectedLODs[I]);
}
