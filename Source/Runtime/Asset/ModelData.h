#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "Math/Matrix.h"
#include "Render/StaticMeshData.h"

constexpr int32 MaxBoneInfluenceCount = 4;

struct FModelNode
{
	FString Name;
	int32 ParentNodeIndex = -1;
	FMatrix LocalTransform = FMatrix::Identity;
};

struct FModelBone
{
	int32 NodeIndex;
	int32 ParentBoneIndex;
	FMatrix InverseBindTransform;
};

struct FModelMesh
{
	int32 NodeIndex;
	int32 FirstSection = -1;
	int32 SectionCount = 0;
};

struct FModelVertexInfluences
{
	int32 BoneIndices[MaxBoneInfluenceCount] = { -1, -1, -1, -1 };
	float Weights[MaxBoneInfluenceCount] = { 0.f, 0.f, 0.f, 0.f };
	int32 InfluenceCount = 0;
};

struct FModelData
{
	bool IsSkeletal;
	FModelMeshData MeshData;
	TArray<FModelVertexInfluences> VertexInfluencesList;
	TArray<FModelNode> Nodes;
	TArray<FModelBone> Bones;
	TArray<FModelMesh> Meshs;
};
