#include "EnginePCH.h"
#include "PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Render/RenderCommand.h"
#include "Render/Mesh.h"
#include "Render/Renderer.h"
#include "Engine/Scene.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
    : Component(InComponent)
{
}

void FPrimitiveSceneProxy::OnMeshRenderDataChanged()
{
    if (Scene)
    {
        Scene->MarkRenderStateDirty(this);
        Scene->MarkDirty(this);
    }
}

void FPrimitiveSceneProxy::OnMeshDestroyed()
{
    RenderState.reset();
}

FBox FPrimitiveSceneProxy::GetLocalBounds() const
{
    return Component->CalcLocalBounds();
}

FLODSphere FPrimitiveSceneProxy::GetLocalLODSphere() const
{
    return FLODSphere();
}

void FPrimitiveSceneProxy::UpdateTransform()
{
    LocalToWorld = GetComponent()->GetWorldMatrix();
    WorldToLocal = LocalToWorld.Inverse();
    NormalMatrix = MakeNormalMatrix(LocalToWorld);

    const FBox LocalBounds = GetLocalBounds();
    Bounds = MakeWorldBounds(LocalBounds.GetWorldAABB(LocalToWorld));

    const FLODSphere LocalSphere = GetLocalLODSphere();
    LODSphere = LocalSphere.Transform(LocalToWorld);
}

void FPrimitiveSceneProxy::UpdateRenderState()
{
    RenderState = MakeShared<FMeshRenderState>();

    UPrimitiveComponent* Component = GetComponent();
    bVisible = Component->IsVisible();
    bCastShadow = Component->IsCastShadow();
    bReceiveShadow = Component->IsReceiveShadow();
    bReceivesDecals = Component->ReceivesDecals();
    bHiddenInGame = Component->IsHiddenInGame();

    ObjectMask = 0;
    if (bCastShadow)
    {
        ObjectMask |= OM_CastShadow;
    }

    if (bReceiveShadow)
    {
        ObjectMask |= OM_ReceiveShadow;
    }

    if (bReceivesDecals)
    {
        ObjectMask |= OM_ReceiveDecals;
    }
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
}

FStaticMeshSceneProxy::~FStaticMeshSceneProxy()
{
    if (!Mesh)
    {
        return;
    }

    Mesh->UnregisterProxy(this);
}

void FStaticMeshSceneProxy::UpdateRenderState()
{
    FPrimitiveSceneProxy::UpdateRenderState();

    UStaticMeshComponent* ActualComponent = Cast<UStaticMeshComponent>(Component);

    UStaticMesh* PreviousMesh = Mesh;
    Mesh = ActualComponent->GetStaticMesh();

    if (Mesh != PreviousMesh)
    {
        if (PreviousMesh)
        {
            PreviousMesh->UnregisterProxy(this);
        }

        if (Mesh)
        {
            Mesh->RegisterProxy(this);
        }

        if (GetScene())
        {
            GetScene()->MarkDirty(this);
        }
    }

    if (!Mesh)
    {
        return;
    }

    auto State = MakeShared<FMeshRenderState>();
    State->bUseSkinning = false;
    State->LODCount = static_cast<uint8>(std::min(Mesh->GetLODCount(), 4u));
    for (uint32 I = 0; I < 3; ++I)
    {
        State->LODThresholdSq[I] = Mesh->ScreenThresholds[I] * Mesh->ScreenThresholds[I];
    }

    for (uint32 LODIndex = 0; LODIndex < State->LODCount; ++LODIndex)
    {
        FCachedMeshLOD& LOD = State->LODs[LODIndex];
        LOD.VertexBuffers[0] = Mesh->GetVertexBuffer(LODIndex);
        LOD.VertexBufferCount = 1;
        LOD.IndexBuffer = Mesh->GetIndexBuffer(LODIndex);
        LOD.FirstSection = State->Sections.Num();

        for (const FMeshSection& Section : Mesh->GetMeshData(LODIndex).Sections)
        {
            UMaterial* Material = Component->GetMaterial(Section.MaterialSlotIndex);
            if (Material)
            {
                State->Sections.Add({ Material, Section.StartIndex, Section.IndexCount });
            }
        }

        LOD.NumSections = State->Sections.Num() - LOD.FirstSection;
    }

    RenderState = State;
}

void FStaticMeshSceneProxy::OnMeshDestroyed()
{
    Mesh = nullptr;
    FPrimitiveSceneProxy::OnMeshDestroyed();
}

FBox FStaticMeshSceneProxy::GetLocalBounds() const
{
    return Mesh ? Mesh->GetRenderBounds() : FPrimitiveSceneProxy::GetLocalBounds();
}

FLODSphere FStaticMeshSceneProxy::GetLocalLODSphere() const
{
    return Mesh ? Mesh->GetLocalLODSphere() : FPrimitiveSceneProxy::GetLocalLODSphere();
}

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
}

FSkeletalMeshSceneProxy::~FSkeletalMeshSceneProxy()
{
}

// 기존 표면 셰이더의 본 행렬 슬롯 연결
void FSkeletalMeshSceneProxy::BindSRVs() const
{
    ID3D11ShaderResourceView* SRV = BoneMatrixSRV.Get();
    RenderCommand::GetContext()->VSSetShaderResources(13, 1, &SRV);
}

bool FSkeletalMeshSceneProxy::UpdateBoneMatrixBuffer()
{
    const auto* ActualComponent = Cast<USkeletalMeshComponent>(GetComponent());
    const TArray<FMatrix>& BoneMatrices = ActualComponent->GetBoneMatrices();
    const uint32 Count = BoneMatrices.Num();
    if (Count == 0)
    {
        BoneMatrixSRV.Reset();
        BoneMatrixBuffer.Reset();
        BoneMatrixCapacity = 0;
        return true;
    }

    ID3D11Device* Device = RenderCommand::GetDevice();
    if (!BoneMatrixBuffer || !BoneMatrixSRV || Count > BoneMatrixCapacity)
    {
        D3D11_BUFFER_DESC BufferDesc{};
        BufferDesc.ByteWidth = Count * sizeof(FMatrix);
        BufferDesc.Usage = D3D11_USAGE_DEFAULT;
        BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        BufferDesc.StructureByteStride = sizeof(FMatrix);

        Microsoft::WRL::ComPtr<ID3D11Buffer> NewBuffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NewSRV;
        HRESULT Result = Device->CreateBuffer(&BufferDesc, nullptr, NewBuffer.GetAddressOf());
        if (FAILED(Result))
        {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.NumElements = Count;
        Result = Device->CreateShaderResourceView(NewBuffer.Get(), &SRVDesc, NewSRV.GetAddressOf());
        if (FAILED(Result))
        {
            return false;
        }

        BoneMatrixBuffer = std::move(NewBuffer);
        BoneMatrixSRV = std::move(NewSRV);
        BoneMatrixCapacity = Count;
    }

    ID3D11DeviceContext* Context = RenderCommand::GetContext();
    const D3D11_BOX UploadRange{ 0, 0, 0, static_cast<UINT>(Count * sizeof(FMatrix)), 1, 1 };
    Context->UpdateSubresource(BoneMatrixBuffer.Get(), 0, &UploadRange, BoneMatrices.GetData(), 0, 0);

    return true;
}

void FSkeletalMeshSceneProxy::UpdateRenderState()
{
    FPrimitiveSceneProxy::UpdateRenderState();

    auto* ActualComponent = Cast<USkeletalMeshComponent>(GetComponent());
    Mesh = ActualComponent->GetSkeletalMesh();

    if (!UpdateBoneMatrixBuffer())
    {
        RenderState.reset();
        return;
    }

    if (!Mesh)
    {
        return;
    }

    RenderState->LODCount = 1;
    RenderState->bUseSkinning = true;

    const FModelMeshData& ModelMeshData = Mesh->GetMeshData();

    for (int32 i = 0; i < ModelMeshData.Sections.Num(); i++)
    {
        const FMeshSection& Section = ModelMeshData.Sections[i];

        FCachedMeshSection& CachedMeshSection = RenderState->Sections.AddDefaulted_GetRef();
        CachedMeshSection.Material = ActualComponent->GetMaterial(Section.MaterialSlotIndex);
        CachedMeshSection.StartIndex = Section.StartIndex;
        CachedMeshSection.IndexCount = Section.IndexCount;
    }

    FCachedMeshLOD& LOD = RenderState->LODs[0];
    LOD.VertexBuffers[0] = Mesh->GetVertexBuffer();
    LOD.VertexBuffers[1] = Mesh->GetSkinWeightBuffer();
    LOD.VertexBufferCount = 2;
    LOD.IndexBuffer = Mesh->GetIndexBuffer();
    LOD.FirstSection = 0;
    LOD.NumSections = ModelMeshData.Sections.Num();
}

void FSkeletalMeshSceneProxy::OnMeshDestroyed()
{
    Mesh = nullptr;
    BoneMatrixSRV.Reset();
    BoneMatrixBuffer.Reset();
    BoneMatrixCapacity = 0;
    FPrimitiveSceneProxy::OnMeshDestroyed();
}

FBox FSkeletalMeshSceneProxy::GetLocalBounds() const
{
	// ����� �ִϸ��̼��� ���� ���� �ٿ�� �ڽ��� �״�� ����. �ִϸ��̼��� ������ �� ������ ����ؾ� �Ѵ�.
	if (!Mesh)
	{
		return FPrimitiveSceneProxy::GetLocalBounds();
	}

    const FModelMeshData& ModelMeshData = Mesh->GetMeshData();
    return ModelMeshData.AABB;
}

FLODSphere FSkeletalMeshSceneProxy::GetLocalLODSphere() const
{
    return FLODSphere();
}
