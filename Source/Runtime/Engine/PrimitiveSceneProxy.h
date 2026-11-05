#pragma once

#include "Math/Frustum.h"
#include "Render/MeshRenderData.h"
#include <d3d11.h>
#include <wrl/client.h>

class FScene;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UMaterial;

enum EObjectMaskBit
{
    OM_CastShadow = 0x1,
    OM_ReceiveShadow = 0x2,
    OM_ReceiveDecals = 0x4
};

class FPrimitiveSceneProxy
{
public:
    explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);
    virtual ~FPrimitiveSceneProxy() = default;

    void UpdateTransform();
    virtual void UpdateRenderState();
    void OnMeshRenderDataChanged();
    virtual void OnMeshDestroyed();
    // 표면 셰이더의 프록시별 자원 연결
    virtual void BindSRVs() const {}
    virtual ID3D11ShaderResourceView* GetBoneMatricesSRV() const { return nullptr; }

    UPrimitiveComponent* GetComponent() const { return Component; }

    const FAABB& GetBounds() const { return Bounds; }
    const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
    const FMatrix& GetWorldToLocal() const { return WorldToLocal; }
    // 노멀 변환용 World 역전치. Transform이 바뀔 때만 다시 계산한다.
    const FMatrix& GetNormalMatrix() const { return NormalMatrix; }
    const FLODSphere& GetLODSphere() const { return LODSphere; }
    bool IsVisible() const { return bVisible; }
    bool IsCastShadow() const { return bCastShadow; }
    bool IsReceiveShadow() const { return bReceiveShadow; }
    bool ReceivesDecals() const{ return bReceivesDecals; }
    bool IsHiddenInGame() const { return bHiddenInGame; }
    uint32 GetObjectMask() const { return ObjectMask; }

    uint8 GetLODCount() const { return RenderState ? RenderState->LODCount : 1; }
    const FMeshRenderState* GetRenderState() const { return RenderState.get(); }
    bool IsUsingSkinning() const { return RenderState && RenderState->bUseSkinning; }
    const float* GetLODThresholdsSq() const { return RenderState->LODThresholdSq; }
    const FCachedMeshLOD& GetLOD(uint32 Index) const { return RenderState->LODs[Index]; }
    const FCachedMeshSection& GetSection(uint32 Index) const { return RenderState->Sections[Index]; }
    FScene* GetScene() const { return Scene; }

protected:
    friend class FScene;

    virtual FBox GetLocalBounds() const;
    virtual FLODSphere GetLocalLODSphere() const;

protected:
    UPrimitiveComponent* Component;
    TSharedPtr<FMeshRenderState> RenderState;

private:
    FScene* Scene = nullptr;
    FMatrix LocalToWorld;
    FMatrix WorldToLocal;
    FMatrix NormalMatrix;
    FAABB Bounds;
    FLODSphere LODSphere;
    int32 PackedIndex = INDEX_NONE;
    bool bQueuedForUpdate = false;
    bool bRenderStateQueued = false;
    bool bVisible = true;
    bool bCastShadow = true;
    bool bReceiveShadow = true;
    bool bReceivesDecals = true;
    bool bHiddenInGame = false;
    uint32 ObjectMask = 0;
};

class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
    FStaticMeshSceneProxy(UStaticMeshComponent* InComponent);
    ~FStaticMeshSceneProxy() override;

    void UpdateRenderState() override;
    void OnMeshDestroyed() override;

protected:
    FBox GetLocalBounds() const override;
    FLODSphere GetLocalLODSphere() const override;

private:
    UStaticMesh* Mesh = nullptr;
};

class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
    FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);
    ~FSkeletalMeshSceneProxy() override;
    void BindSRVs() const override;

    void UpdateRenderState() override;
    void OnMeshDestroyed() override;
    bool UpdateBoneMatrixBuffer();

    ID3D11ShaderResourceView* GetBoneMatricesSRV() const override
    {
        return BoneMatrixSRV.Get();
    }

protected:
    FBox GetLocalBounds() const override;
    FLODSphere GetLocalLODSphere() const override;

private:
    USkeletalMesh* Mesh = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> BoneMatrixBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> BoneMatrixSRV;
    uint32 BoneMatrixCapacity = 0;
};
