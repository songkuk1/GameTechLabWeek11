#include "EnginePCH.h"
#include "Render/SkeletalMesh.h"

#include "Animation/Skeleton.h"
#include "Render/RenderCommand.h"

void USkeletalMesh::Initialize(const FModelMeshData& InMeshData, const TArray<FModelVertexInfluences>& InVertexInfluencesList, const TArray<FModelMesh>& InMeshs)
{
	MeshData = InMeshData;
	VertexInfluencesList = InVertexInfluencesList;
	Meshs = InMeshs;

	VertexBuffer = RenderCommand::CreateStaticVertexBuffer(InMeshData.Vertices.GetData(), sizeof(FVertexPNCT) * static_cast<uint32>(InMeshData.Vertices.size()), sizeof(FVertexPNCT));
	SkinWeightBuffer = RenderCommand::CreateStaticVertexBuffer(InVertexInfluencesList.GetData(), sizeof(FModelVertexInfluences) * InVertexInfluencesList.size(), sizeof(FModelVertexInfluences));
	IndexBuffer = RenderCommand::CreateStaticIndexBuffer(InMeshData.Indices.GetData(), static_cast<uint32>(InMeshData.Indices.size()));
}

void USkeletalMesh::SetSkeleton(USkeleton* InSkeleton)
{
	Skeleton = InSkeleton;
}

USkeleton* USkeletalMesh::GetSkeleton()
{
	return Skeleton;
}

void USkeletalMesh::SetMaterials(const TArray<UMaterial*> InMaterials)
{
	Materials = InMaterials;
}

const TArray<UMaterial*>& USkeletalMesh::GetMaterials()
{
	return Materials;
}

const FModelMeshData& USkeletalMesh::GetMeshData() const
{
	return MeshData;
}

FVertexBuffer* USkeletalMesh::GetVertexBuffer() const
{
	return VertexBuffer.get();
}

FVertexBuffer* USkeletalMesh::GetSkinWeightBuffer() const
{
	return SkinWeightBuffer.get();
}

FIndexBuffer* USkeletalMesh::GetIndexBuffer() const
{
	return IndexBuffer.get();
}
