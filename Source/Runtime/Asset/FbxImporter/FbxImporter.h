#pragma once

#include "Asset/ModelData.h"
#include "Containers/Map.h"
#include <filesystem>
#include <fbxsdk.h>

class FFbxImporter
{
public:
	static bool LoadFromFile(const FString& Path, FModelData& OutModelData);

private:
	struct FMeshNode
	{
		fbxsdk::FbxNode* FbxNode;
		int32 NodeIndex;
	};

	struct FSoftModelVertexInfluences
	{
		TArray<int32> BoneIndices;
		TArray<float> Weights;
	};

	struct FTriangleFace
	{
		uint32 VertexIndices[3];
		uint32 MaterialSlotIndex;
	};

	static void BuildNodeHierarchy(int32 ParentNodeIndex, fbxsdk::FbxNode* Current, TMap<uint64, int32>& NodeToIndexMap, TMap<int32, int32>& NodeIndexToBoneIndexMap, TArray<FModelNode>& OutNodes, TArray<FModelBone>& OutBones, TArray<FMeshNode>& OutMeshNodes);

	static void ProcessMeshNode(const std::filesystem::path& FbxPath, const TMap<uint64, int32>& NodeToIndexMap, const TMap<int32, int32>& NodeIndexToBoneIndexMap, TArray<FModelBone>& Bones, const FMeshNode& MeshNode, FModelMeshData& OutModelMeshData, TArray<FModelVertexInfluences>& OutVertexInfluencesList, TArray<FModelMesh>& OutMeshs);

	static void ParseMaterial(const std::filesystem::path& ParentPath, fbxsdk::FbxSurfaceMaterial* Material, FMeshMaterialSlot& OutMaterial);

	static void ParseMesh(const std::filesystem::path& FbxPath, fbxsdk::FbxMesh* Mesh, TArray<FVertexPNCT>& OutVertices, TArray<int32>& OutVertexToControlPoints, TArray<FMeshMaterialSlot>& OutMaterials, TArray<FTriangleFace>& OutFaces);

	static void CompactSoftSkin(FSoftModelVertexInfluences& Skin);

	static void ParseSkin(const TArray<int32>& VertexToControlPoints, const TMap<uint64, int32>& NodeToIndexMap, const TMap<int32, int32>& NodeIndexToBoneIndexMap, const FMeshNode& MeshNode, TArray<FModelBone>& Bones, TArray<FModelVertexInfluences>& OutVertexInfluencesList, FMatrix& OutBindGlobalTransform);
};
