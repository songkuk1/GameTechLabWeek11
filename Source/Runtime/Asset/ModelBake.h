#pragma once

#include "Asset/ModelData.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveSlots.h"

class FModelBake
{
public:
	static bool ReadBaked(const FString& Path, FModelData& OutModelData);

	static bool WriteBaked(const FString& Path, FModelData& ModelData);

private:
	static void SerializeModelNode(FStructuredArchive::FRecord& Record, FModelNode& Node);

	static void SerializeModelBone(FStructuredArchive::FRecord& Record, FModelBone& Bone);

	static void SerializeModelMesh(FStructuredArchive::FRecord& Record, FModelMesh& Mesh);

	static void SerializeQuat(FStructuredArchive::FRecord& Record, FQuat& Quat);

	static void SerializeModelAnimationTrack(FStructuredArchive::FRecord& Record, FModelAnimationTrack& Track);

	static void SerializeModelAnimationCurveKey(FStructuredArchive::FRecord& Record, FAnimationCurveKey& Key);

	static void SerializeModelAnimationCurve(FStructuredArchive::FRecord& Record, FAnimationCurve& Curve);

	static void SerializeModelAnimation(FStructuredArchive::FRecord& Record, FModelAnimation& Animation);

	static void SerializeModelVertexInfluences(FStructuredArchive::FRecord& Record, FModelVertexInfluences& Influences);

	static void SerializeVertex(FStructuredArchive::FRecord& Record, FVertexPNCT& Vertex);

	static void SerializeMeshSection(FStructuredArchive::FRecord& Record, FMeshSection& Section);

	static void SerializeMaterialSlot(FStructuredArchive::FRecord& Record, FMeshMaterialSlot& Slot);

	template <typename ElementType, typename ElementSerializer>
	static void SerializeArray(FStructuredArchive::FRecord& Record, const char* Name, TArray<ElementType>& Array, ElementSerializer Serializer);

	static void SerializeModelData(FStructuredArchive::FRecord& Record, FModelData& ModelData);
};
