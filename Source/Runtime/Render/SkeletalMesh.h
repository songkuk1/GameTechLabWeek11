#pragma once

#include "Asset/Asset.h"
#include "Asset/ModelData.h"
#include "Render/Buffer.h"

class USkeleton;
class UMaterial;

class USkeletalMesh : public UAsset
{
	DECLARE_CLASS(USkeletalMesh, UAsset)

public:
	void Initialize(const FModelMeshData& InMeshData, const TArray<FModelVertexInfluences>& InVertexInfluencesList, const TArray<FModelMesh>& InMeshs);

	void SetSkeleton(USkeleton* InSkeleton);

	USkeleton* GetSkeleton();

	void SetMaterials(const TArray<UMaterial*> InMaterials);

	const TArray<UMaterial*>& GetMaterials();

	const FModelMeshData& GetMeshData() const;

	FVertexBuffer* GetVertexBuffer() const;

	FVertexBuffer* GetSkinWeightBuffer() const;

	FIndexBuffer* GetIndexBuffer() const;

private:
	USkeleton* Skeleton = nullptr;

	FModelMeshData MeshData;
	TArray<FModelVertexInfluences> VertexInfluencesList;
	TArray<FModelMesh> Meshs;

	TArray<UMaterial*> Materials;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FVertexBuffer> SkinWeightBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;
};
