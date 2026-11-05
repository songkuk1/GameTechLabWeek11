#include "EnginePCH.h"
#include "Animation/Skeleton.h"

#include "Core/PathUtils.h"
#include "Json/Dom/JsonValue.h"

#include <cmath>

namespace
{
	constexpr int32 PoseVersion = 1;
	constexpr int32 MatrixElementCount = 16;

	// 임시 파일 기록 이후의 포즈 파일 교체
	bool WritePoseFile(const std::filesystem::path& Path, const FJsonValue& Document)
	{
		const FString Contents = Document.dump(2, ' ', false, FJsonValue::error_handler_t::replace);
		std::filesystem::path TempPath = Path;
		TempPath += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
		std::ofstream File(TempPath, std::ios::binary | std::ios::trunc);
		if (!File)
		{
			return false;
		}

		File << Contents << '\n';
		File.close();
		if (!File || !MoveFileExW(TempPath.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::error_code Error;
			std::filesystem::remove(TempPath, Error);
			return false;
		}
		return true;
	}

	// 본 식별 정보 및 유한 행렬 값 검증
	bool ReadBonePose(const FJsonValue& Entry, const FModelBone& Bone, const FModelNode& Node, FMatrix& OutTransform)
	{
		if (!Entry.contains("NodeIndex") || Entry["NodeIndex"] != Bone.NodeIndex || !Entry.contains("Name") || Entry["Name"] != Node.Name || !Entry.contains("ParentNodeIndex") || Entry["ParentNodeIndex"] != Node.ParentNodeIndex || !Entry.contains("LocalTransform"))
		{
			return false;
		}

		const FJsonValue& Values = Entry["LocalTransform"];
		if (!Values.is_array() || Values.size() != MatrixElementCount)
		{
			return false;
		}

		for (int32 Index = 0; Index < MatrixElementCount; ++Index)
		{
			if (!Values[Index].is_number())
			{
				return false;
			}

			const float Value = Values[Index].get<float>();
			if (!std::isfinite(Value))
			{
				return false;
			}

			OutTransform.M[Index / 4][Index % 4] = Value;
		}
		return true;
	}
}

void USkeleton::Initialize(const TArray<FModelNode>& InNodes, const TArray<FModelBone>& InBones)
{
	Nodes = InNodes;
	Bones = InBones;
	DefaultPosePath.clear();
	PosePath.clear();
}

// 모델 경로에 연결된 기본 포즈 복원
void USkeleton::LoadDefaultPose(const FString& SourcePath)
{
	DefaultPosePath = std::filesystem::absolute(PathUtils::FromUtf8(SourcePath + ".pose")).lexically_normal();
	PosePath = DefaultPosePath;
	std::ifstream File(PosePath, std::ios::binary);
	if (!File)
	{
		return;
	}

	auto Document = FJsonValue::parse(File, nullptr, false);
	if (Document.contains("Version") && Document["Version"] == PoseVersion && Document.contains("PosePath") && Document["PosePath"].is_string())
	{
		const auto SavedPath = PathUtils::FromUtf8(Document["PosePath"].get<FString>());
		PosePath = (DefaultPosePath.parent_path() / SavedPath).lexically_normal();
		std::ifstream PoseFile(PosePath, std::ios::binary);
		Document = FJsonValue::parse(PoseFile, nullptr, false);
	}

	if (!Document.contains("Version") || Document["Version"] != PoseVersion || !Document.contains("Bones") || !Document["Bones"].is_array() || Document["Bones"].size() != static_cast<size_t>(Bones.Num()))
	{
		HTR_LOG(Warning, "Invalid skeletal mesh pose: {}", PathUtils::ToUtf8(PosePath));
		return;
	}

	TArray<FMatrix> LocalTransforms;
	LocalTransforms.SetNum(Bones.Num());
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		const FModelBone& Bone = Bones[Index];
		if (!Nodes.IsValidIndex(Bone.NodeIndex) || !ReadBonePose(Document["Bones"][Index], Bone, Nodes[Bone.NodeIndex], LocalTransforms[Index]))
		{
			HTR_LOG(Warning, "Skeletal mesh pose does not match the imported skeleton: {}", PathUtils::ToUtf8(PosePath));
			return;
		}
	}

	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		Nodes[Bones[Index].NodeIndex].LocalTransform = LocalTransforms[Index];
	}
}

// 로컬 본 포즈 저장 및 모델의 포즈 경로 연결
bool USkeleton::SaveDefaultPose(const TArray<FMatrix>& LocalPose, const std::filesystem::path& Destination)
{
	if (DefaultPosePath.empty() || Destination.empty() || _wcsicmp(Destination.extension().c_str(), L".pose") != 0 || Bones.IsEmpty() || LocalPose.Num() != Nodes.Num())
	{
		return false;
	}

	std::error_code Error;
	std::filesystem::path SavePath = std::filesystem::weakly_canonical(Destination, Error);
	if (Error)
	{
		return false;
	}

	const auto ResolvedDefault = std::filesystem::weakly_canonical(DefaultPosePath, Error);
	const bool bUseDefaultPath = !Error && _wcsicmp(SavePath.c_str(), ResolvedDefault.c_str()) == 0;
	if (bUseDefaultPath)
	{
		SavePath = DefaultPosePath;
	}

	FJsonValue Document{ { "Version", PoseVersion }, { "Bones", FJsonValue::array() } };
	FJsonValue& Entries = Document["Bones"];
	for (const FModelBone& Bone : Bones)
	{
		if (!Nodes.IsValidIndex(Bone.NodeIndex))
		{
			return false;
		}

		auto Values = FJsonValue::array();
		for (const auto& Row : LocalPose[Bone.NodeIndex].M)
		{
			for (const float Value : Row)
			{
				if (!std::isfinite(Value))
				{
					return false;
				}
				Values.push_back(Value);
			}
		}

		const FModelNode& Node = Nodes[Bone.NodeIndex];
		Entries.push_back({ { "NodeIndex", Bone.NodeIndex }, { "Name", Node.Name }, { "ParentNodeIndex", Node.ParentNodeIndex }, { "LocalTransform", std::move(Values) } });
	}

	if (!WritePoseFile(SavePath, Document))
	{
		return false;
	}

	if (!bUseDefaultPath && SavePath != PosePath)
	{
		const auto RelativePath = SavePath.lexically_relative(DefaultPosePath.parent_path());
		const FJsonValue Reference{ { "Version", PoseVersion }, { "PosePath", PathUtils::ToUtf8(RelativePath.empty() ? SavePath : RelativePath) } };
		if (!WritePoseFile(DefaultPosePath, Reference))
		{
			HTR_LOG(Error, "Pose saved to {}, but the asset reference could not be saved: {}", PathUtils::ToUtf8(SavePath), PathUtils::ToUtf8(DefaultPosePath));
			return false;
		}
	}

	PosePath = SavePath;
	for (const FModelBone& Bone : Bones)
	{
		Nodes[Bone.NodeIndex].LocalTransform = LocalPose[Bone.NodeIndex];
	}
	return true;
}

// 현재 적용된 포즈 파일 경로 조회
const std::filesystem::path& USkeleton::GetPosePath() const
{
	return PosePath;
}

const TArray<FModelNode>& USkeleton::GetNodes() const
{
	return Nodes;
}

const TArray<FModelBone>& USkeleton::GetBones() const
{
	return Bones;
}
