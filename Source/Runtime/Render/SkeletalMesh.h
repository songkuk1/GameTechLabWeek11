#pragma once

#include "Asset/Asset.h"
#include "Asset/ModelData.h"
#include "Render/Buffer.h"
#include "SkeletalMesh.generated.h"

class USkeleton;
class UMaterial;

UCLASS()
class USkeletalMesh : public UAsset
{
	GENERATED_BODY()

public:
	void Initialize(const FModelMeshData& InMeshData, const TArray<FModelVertexInfluences>& InVertexInfluencesList, const TArray<FModelMesh>& InMeshs);

	void SetSkeleton(USkeleton* InSkeleton);

	USkeleton* GetSkeleton();

	void SetMaterials(const TArray<UMaterial*> InMaterials);

	const TArray<UMaterial*>& GetMaterials();

	const FModelMeshData& GetMeshData() const;

	const TArray<FModelVertexInfluences>& GetVertexInfluencesList() const { return VertexInfluencesList; }

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
