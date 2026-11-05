#pragma once

#include "Asset/Asset.h"
#include "Render/Buffer.h"
#include "Render/StaticMeshData.h"
#include "Render/MeshRenderData.h"

class FPrimitiveSceneProxy;

struct FStaticMeshLODResource
{
	FModelMeshData Data;
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;
};

class UStaticMesh : public UAsset
{
	DECLARE_CLASS(UStaticMesh, UAsset)
public:
    UStaticMesh() : SortID(NextSortID++) {}
	virtual ~UStaticMesh() override;

	FModelMeshData MeshData;

	// Renderer가 실제로 Bind할 런타임 재질 객체
	TArray<UMaterial*> Materials;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;

	const FModelMeshData& GetMeshData() const { return MeshData; }
	UMaterial* GetMaterial(uint32 SlotIndex) const;

public:
    TArray<FStaticMeshLODResource> AdditionalLODs;
    std::array<float, 3> ScreenThresholds{ 0.30f, 0.10f, 0.03f };
    FString SourceObjPath;

    uint32 GetLODCount() const
    {
        return 1u + static_cast<uint32>(AdditionalLODs.Num());
    }

    const FModelMeshData& GetMeshData(uint32 LODIndex) const
    {
        return LODIndex == 0 ? MeshData : AdditionalLODs[LODIndex - 1].Data;
    }

    FVertexBuffer* GetVertexBuffer(uint32 LODIndex) const
    {
        return LODIndex == 0 ? VertexBuffer.get() : AdditionalLODs[LODIndex - 1].VertexBuffer.get();
    }

    FIndexBuffer* GetIndexBuffer(uint32 LODIndex) const
    {
        return LODIndex == 0 ? IndexBuffer.get() : AdditionalLODs[LODIndex - 1].IndexBuffer.get();
    }

    void RebuildRenderData();
    const FLODSphere& GetLocalLODSphere() const { return LocalLODSphere; }
    const FBox& GetRenderBounds() const { return RenderBounds; }
    void RegisterProxy(FPrimitiveSceneProxy* Proxy);
    void UnregisterProxy(FPrimitiveSceneProxy* Proxy);

    const uint16 SortID;
private:
    static inline uint16 NextSortID = 0;
    FLODSphere LocalLODSphere;
    FBox RenderBounds;
    TArray<FPrimitiveSceneProxy*> RenderProxies;
};
