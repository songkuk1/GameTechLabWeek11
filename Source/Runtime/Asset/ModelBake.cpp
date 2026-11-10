#include "EnginePCH.h"
#include "Asset/ModelBake.h"

#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/Formatter/BinaryArchiveFormatter.h"
#include "Serialization/MathSerialization.h"
#include "Core/PathUtils.h"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <limits>

namespace
{
constexpr uint32 ModelBakeVersion = 5;

struct FModelBakeHeader
{
	char Magic[4] = { 'M', 'O', 'D', 'B' }; // 'M''o'del 'D'ata 'B'ake
	uint32 Version = ModelBakeVersion;
};

constexpr uint32 AnimationAssetVersion = 1;

struct FAnimationAssetHeader
{
	char Magic[4] = { 'A', 'N', 'I', 'M' };
	uint32 Version = AnimationAssetVersion;
};
}

template <typename ElementType, typename ElementSerializer>
void FModelBake::SerializeArray(FStructuredArchive::FRecord& Record, const char* Name, TArray<ElementType>& Array, ElementSerializer Serializer)
{
	int32 Count = Array.Num();

	auto ArrayRecord = Record.EnterArray(Name, Count);
	if (Record.GetUnderlyingArchive().IsLoading())
	{
		Array.SetNum(Count);
	}

	for (int32 i = 0; i < Count; ++i)
	{
		auto ElementRecord = ArrayRecord.EnterElement().EnterRecord();
		Serializer(ElementRecord, Array[i]);
	}
}

bool FModelBake::ReadBaked(const FString& Path, FModelData& OutModelData)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		return false;   // .bin이 없음 (처음 로드)
	}

	int32 FileSize = static_cast<int32>(std::filesystem::file_size(Path));

	TArray<uint8> Bytes;
	Bytes.SetNum(FileSize);
	In.read(reinterpret_cast<char*>(Bytes.GetData()), FileSize);

	FMemoryReader Reader(Bytes);
	if (Reader.IsError())
	{
		return false;
	}

	FModelBakeHeader Header;
	Reader.Serialize(&Header, sizeof(Header));

	const FModelBakeHeader Expected;
	if (std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 || Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-cooking", Path);
		return false;
	}

	FBinaryArchiveFormatter Formatter(Reader);
	FStructuredArchive Archive(Formatter);

	auto Root = Archive.Open().EnterRecord();
	SerializeModelData(Root, OutModelData);
	Archive.Close();

	if (Reader.IsError())
	{
		HTR_LOG(Warning, "[BAKE] {}: failed to read data, re-cooking", Path);
		return false;
	}

	OutModelData.MeshData.BuildAABB();
	OutModelData.MeshData.BuildTriangleBVH();

	return true;
}

bool FModelBake::WriteBaked(const FString& Path, FModelData& ModelData)
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);

	FModelBakeHeader Header;
	Writer.Serialize(&Header, sizeof(Header));

	FBinaryArchiveFormatter Formatter(Writer);
	FStructuredArchive Archive(Formatter);

	auto Root = Archive.Open().EnterRecord();
	SerializeModelData(Root, ModelData);
	Archive.Close();

	if (Writer.IsError())
	{
		HTR_LOG(Warning, "[BAKE] {}: failed to write data", Path);
		return false;
	}

	std::ofstream Out(Path, std::ios::binary);
	if (!Out)
	{
		return false;
	}

	Out.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());

	return true;
}

bool FModelBake::WriteAnimationAsset(const FString& Path, const FString& SourceMeshKey, FModelAnimation& Animation)
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);

	FAnimationAssetHeader Header;
	Writer.Serialize(&Header, sizeof(Header));

	FBinaryArchiveFormatter Formatter(Writer);
	FStructuredArchive Archive(Formatter);

	auto Root = Archive.Open().EnterRecord();
	FString StoredSourceMeshKey = SourceMeshKey;
	Root << SA_VALUE("SourceMeshKey", StoredSourceMeshKey);
	SerializeModelAnimation(Root, Animation);
	Archive.Close();

	if (Writer.IsError())
	{
		HTR_LOG(Warning, "[ANIMATION] {}: failed to serialize animation.", Path);
		return false;
	}

	std::ofstream Out(PathUtils::FromUtf8(Path), std::ios::binary | std::ios::trunc);
	if (!Out)
	{
		HTR_LOG(Warning, "[ANIMATION] {}: cannot open for writing.", Path);
		return false;
	}

	Out.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
	return !Out.fail();
}

bool FModelBake::ReadAnimationAsset(const FString& Path, FString& OutSourceMeshKey, FModelAnimation& OutAnimation)
{
	const std::filesystem::path FilePath = PathUtils::FromUtf8(Path);

	if (!fs::exists(FilePath))
	{
		return false;
	}

	std::ifstream In(FilePath, std::ios::binary);
	if (!In)
	{
		return false;
	}

	const uintmax_t FileSize = fs::file_size(FilePath);
	if (FileSize < sizeof(FAnimationAssetHeader) || FileSize > static_cast<uintmax_t>(INT32_MAX))
	{
		return false;
	}

	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(FileSize));
	In.read(reinterpret_cast<char*>(Bytes.GetData()), Bytes.Num());

	if (!In)
	{
		return false;
	}

	FMemoryReader Reader(Bytes);

	FAnimationAssetHeader Header;
	Reader.Serialize(&Header, sizeof(Header));

	const FAnimationAssetHeader Expected;
	if (Reader.IsError() || std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 || Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[ANIMATION] {}: invalid header or version.", Path);
		return false;
	}

	OutSourceMeshKey.clear();
	OutAnimation = FModelAnimation{};

	FBinaryArchiveFormatter Formatter(Reader);
	FStructuredArchive Archive(Formatter);

	auto Root = Archive.Open().EnterRecord();
	Root << SA_VALUE("SourceMeshKey", OutSourceMeshKey);
	SerializeModelAnimation(Root, OutAnimation);
	Archive.Close();

	if (Reader.IsError() || OutSourceMeshKey.empty() || !std::isfinite(OutAnimation.PlayLength) || OutAnimation.PlayLength < 0.0f || OutAnimation.FrameRateNumerator <= 0 || OutAnimation.FrameRateDenominator <= 0 || OutAnimation.NumberOfFrames <= 0)
	{
		HTR_LOG(Warning, "[ANIMATION] {}: invalid animation data.", Path);
		return false;
	}

	return true;
}

void FModelBake::SerializeModelNode(FStructuredArchive::FRecord& Record, FModelNode& Node)
{
	Record << SA_VALUE("Name", Node.Name);
	Record << SA_VALUE("ParentNodeIndex", Node.ParentNodeIndex);
	Record << SA_VALUE("LocalTransform", Node.LocalTransform);
}

void FModelBake::SerializeModelBone(FStructuredArchive::FRecord& Record, FModelBone& Bone)
{
	Record << SA_VALUE("NodeIndex", Bone.NodeIndex);
	Record << SA_VALUE("ParentBoneIndex", Bone.ParentBoneIndex);
	Record << SA_VALUE("InverseBindTransform", Bone.InverseBindTransform);
}

void FModelBake::SerializeModelMesh(FStructuredArchive::FRecord& Record, FModelMesh& Mesh)
{
	Record << SA_VALUE("NodeIndex", Mesh.NodeIndex);
	Record << SA_VALUE("FirstSection", Mesh.FirstSection);
	Record << SA_VALUE("SectionCount", Mesh.SectionCount);
}

void FModelBake::SerializeQuat(FStructuredArchive::FRecord& Record, FQuat& Quat)
{
	Record << SA_VALUE("X", Quat.X);
	Record << SA_VALUE("Y", Quat.Y);
	Record << SA_VALUE("Z", Quat.Z);
	Record << SA_VALUE("W", Quat.W);
}

void FModelBake::SerializeModelAnimationTrack(FStructuredArchive::FRecord& Record, FModelAnimationTrack& Track)
{
	Record << SA_VALUE("Name", Track.Name);
	Record << SA_VALUE("NodeIndex", Track.NodeIndex);
	SerializeArray<FVector>(Record, "PosKeys", Track.PosKeys, [](FStructuredArchive::FRecord& ElementRecord, FVector& Value) {ElementRecord << SA_VALUE("Value", Value);});
	SerializeArray<FQuat>(Record, "RotKeys", Track.RotKeys, [](FStructuredArchive::FRecord& ElementRecord, FQuat& Value) {FModelBake::SerializeQuat(ElementRecord, Value);});
	SerializeArray<FVector>(Record, "ScaleKeys", Track.ScaleKeys, [](FStructuredArchive::FRecord& ElementRecord, FVector& Value) {ElementRecord << SA_VALUE("Value", Value);});
}

void FModelBake::SerializeModelAnimationCurveKey(FStructuredArchive::FRecord& Record, FAnimationCurveKey& Key)
{
	uint8 Interpolation = static_cast<uint8>(Key.Interpolation);

	Record << SA_VALUE("Time", Key.Time);
	Record << SA_VALUE("Value", Key.Value);
	Record << SA_VALUE("ArriveTangent", Key.ArriveTangent);
	Record << SA_VALUE("LeaveTangent", Key.LeaveTangent);
	Record << SA_VALUE("Interpolation", Interpolation);

	if (Record.GetUnderlyingArchive().IsLoading())
	{
		if (Interpolation > static_cast<uint8>(EAnimationCurveInterpolation::Cubic))
		{
			Key.Interpolation = EAnimationCurveInterpolation::Linear;
		}
		else
		{
			Key.Interpolation = static_cast<EAnimationCurveInterpolation>(Interpolation);
		}
	}
}

void FModelBake::SerializeModelAnimationCurve(FStructuredArchive::FRecord& Record, FAnimationCurve& Curve)
{
	Record << SA_VALUE("Name", Curve.Name);
	Record << SA_VALUE("LayerName", Curve.LayerName);
	Record << SA_VALUE("NodeIndex", Curve.NodeIndex);
	SerializeArray<FAnimationCurveKey>(Record, "Keys", Curve.Keys, SerializeModelAnimationCurveKey);
}

void FModelBake::SerializeModelAnimation(FStructuredArchive::FRecord& Record, FModelAnimation& Animation)
{
	Record << SA_VALUE("Name", Animation.Name);
	Record << SA_VALUE("PlayLength", Animation.PlayLength);
	Record << SA_VALUE("FrameRateNumerator", Animation.FrameRateNumerator);
	Record << SA_VALUE("FrameRateDenominator", Animation.FrameRateDenominator);
	Record << SA_VALUE("NumberOfFrames", Animation.NumberOfFrames);
	Record << SA_VALUE("NumberOfKeys", Animation.NumberOfKeys);
	SerializeArray<FModelAnimationTrack>(Record, "Tracks", Animation.Tracks, SerializeModelAnimationTrack);
	SerializeArray<FAnimationCurve>(Record, "Curves", Animation.CurveData.Curves, SerializeModelAnimationCurve);
}

void FModelBake::SerializeModelVertexInfluences(FStructuredArchive::FRecord& Record, FModelVertexInfluences& Influences)
{
	FArchive& Ar = Record.GetUnderlyingArchive();
	Record << SA_VALUE("InfluenceCount", Influences.InfluenceCount);

	if (Influences.InfluenceCount < 0 || Influences.InfluenceCount > MaxBoneInfluenceCount)
	{
		return;
	}

	int32 IndexCount = Influences.InfluenceCount;
	auto IndicesArrayRecord = Record.EnterArray("BoneIndices", IndexCount);
	if (IndexCount != Influences.InfluenceCount)
	{
		return;
	}

	for (int32 i = 0; i < IndexCount; ++i)
	{
		auto ElementRecord = IndicesArrayRecord.EnterElement().EnterRecord();
		ElementRecord << SA_VALUE("BoneIndex", Influences.BoneIndices[i]);
	}

	int32 WeightCount = Influences.InfluenceCount;
	auto WeightsArrayRecord = Record.EnterArray("Weights", WeightCount);
	if (WeightCount != Influences.InfluenceCount)
	{
		return;
	}

	for (int32 i = 0; i < WeightCount; ++i)
	{
		auto ElementRecord = WeightsArrayRecord.EnterElement().EnterRecord();
		ElementRecord << SA_VALUE("Weight", Influences.Weights[i]);
	}
}

void FModelBake::SerializeVertex(FStructuredArchive::FRecord& Record, FVertexPNCT& Vertex)
{
	Record << SA_VALUE("Position", Vertex.Position);
	Record << SA_VALUE("Normal", Vertex.Normal);
	Record << SA_VALUE("Tangent", Vertex.Tangent);
	Record << SA_VALUE("Color", Vertex.Color);
	Record << SA_VALUE("UV", Vertex.UV);
}

void FModelBake::SerializeMeshSection(FStructuredArchive::FRecord& Record, FMeshSection& Section)
{
	Record << SA_VALUE("StartIndex", Section.StartIndex);
	Record << SA_VALUE("IndexCount", Section.IndexCount);
	Record << SA_VALUE("MaterialSlotIndex", Section.MaterialSlotIndex);
}

void FModelBake::SerializeMaterialSlot(FStructuredArchive::FRecord& Record, FMeshMaterialSlot& Slot)
{
	Record << SA_VALUE("Name", Slot.Name);
	Record << SA_VALUE("BaseColor", Slot.BaseColor);
	Record << SA_VALUE("DiffuseTexturePath", Slot.DiffuseTexturePath);
	Record << SA_VALUE("NormalTexturePath", Slot.NormalTexturePath);
	Record << SA_VALUE("Shininess", Slot.Shininess);
	Record << SA_VALUE("SpecularColor", Slot.SpecularColor);
}

void FModelBake::SerializeModelData(FStructuredArchive::FRecord& Record, FModelData& ModelData)
{
	Record << SA_VALUE("IsSkeletal", ModelData.IsSkeletal);

	SerializeArray<FVertexPNCT>(Record, "Vertices", ModelData.MeshData.Vertices, SerializeVertex);
	SerializeArray<uint32>(Record, "Indices", ModelData.MeshData.Indices, [](FStructuredArchive::FRecord& Record, uint32& Value) { Record << SA_VALUE("Index", Value); });
	SerializeArray<FMeshSection>(Record, "Sections", ModelData.MeshData.Sections, SerializeMeshSection);
	SerializeArray<FMeshMaterialSlot>(Record, "MaterialSlots", ModelData.MeshData.MaterialSlots, SerializeMaterialSlot);
	SerializeArray<FModelNode>(Record, "Nodes", ModelData.Nodes, SerializeModelNode);
	SerializeArray<FModelBone>(Record, "Bones", ModelData.Bones, SerializeModelBone);
	SerializeArray<FModelMesh>(Record, "Meshs", ModelData.Meshs, SerializeModelMesh);
	SerializeArray<FModelVertexInfluences>(Record, "VertexInfluencesList", ModelData.VertexInfluencesList, SerializeModelVertexInfluences);
	SerializeArray<FModelAnimation>(Record, "Animation", ModelData.Animations, SerializeModelAnimation);
}
