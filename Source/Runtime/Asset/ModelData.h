#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "Math/Matrix.h"
#include "Render/StaticMeshData.h"
#include "Core/NameTypes.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Animation/AnimationCurve.h"

constexpr int32 MaxBoneInfluenceCount = 4;

struct FModelNode
{
	FString Name;
	int32 ParentNodeIndex = -1;
	FMatrix LocalTransform = FMatrix::Identity;
};

struct FModelBone
{
	int32 NodeIndex = -1;
	int32 ParentBoneIndex = -1;
	FMatrix InverseBindTransform = FMatrix::Identity;
};

struct FModelMesh
{
	int32 NodeIndex = -1;
	int32 FirstSection = -1;
	int32 SectionCount = 0;
};

struct FModelVertexInfluences
{
	int32 BoneIndices[MaxBoneInfluenceCount] = { -1, -1, -1, -1 };
	float Weights[MaxBoneInfluenceCount] = { 0.f, 0.f, 0.f, 0.f };
	int32 InfluenceCount = 0;
};

struct FModelAnimationTrack
{
	FName Name;
	int32 NodeIndex = -1;
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	TArray<FVector> ScaleKeys;
};

struct FModelAnimation
{
	FName Name;
	float PlayLength = 0.0f;
	int32 FrameRateNumerator = 30;
	int32 FrameRateDenominator = 1;
	int32 NumberOfFrames = 0;
	int32 NumberOfKeys = 0;
	TArray<FModelAnimationTrack> Tracks;
	FAnimationCurveData CurveData;
};

struct FModelData
{
	bool IsSkeletal = false;
	FModelMeshData MeshData;
	TArray<FModelVertexInfluences> VertexInfluencesList;
	TArray<FModelNode> Nodes;
	TArray<FModelBone> Bones;
	TArray<FModelMesh> Meshs;
	TArray<FModelAnimation> Animations;
};
