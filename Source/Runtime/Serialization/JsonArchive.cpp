#include "EnginePCH.h"
#include "JsonArchive.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "TypeSerializer.h"
#include "Asset/AssetManager.h"
#include "Sequence/MovieSceneSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimationGraph.h"
#include "Animation/AnimSequence.h"
#include "Core/PathUtils.h"

#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Serialization/Formatter/JsonArchiveOutputFormatter.h"
#include "Serialization/Formatter/JsonArchiveInputFormatter.h"

namespace
{
	constexpr int32 SceneVersion = 4;   // 포맷이 바뀌었으므로 올린다
	constexpr int32 SequenceVersion = 2;
	constexpr int32 BlendSpaceVersion = 1;
	constexpr int32 AnimationGraphVersion = 1;
	constexpr int32 AnimNotifyVersion = 1;
}

bool FJsonArchive::SaveWorldToBytes(UWorld* World, TArray<uint8>& OutBytes)
{
	if (!World)
		return false;

	FMemoryWriter Writer(OutBytes);
	{
		// 선언 순서 = 소멸 역순: Ar(Close) → Formatter(dump → Writer)
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();
		int32 Version = SceneVersion;
		Root << SA_VALUE("Version", Version);
		World->Serialize(Root.EnterRecord("World"));

		Ar.Close();
	} 

	return !Writer.IsError();
}

bool FJsonArchive::LoadWorldFromBytes(UWorld* World, const TArray<uint8>& Bytes)
{
	if (!World)
		return false;

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);   
	if (Reader.IsError())
	{
		HTR_LOG(Error, "Load: invalid scene data");
		return false;                               
	}

	FStructuredArchive Ar(Formatter);
	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

	int32 Version = 0;
	Root << SA_VALUE("Version", Version);
	if (Version != SceneVersion)
	{
		HTR_LOG(Error, "Load: scene version {} is not supported (expected {})", Version, SceneVersion);
		return false;                               
	}

	World->ClearWorld();
	World->Serialize(Root.EnterRecord("World"));
	Ar.Close();

	World->GetScene().BuildBVH();
	return !Reader.IsError();
}

bool FJsonArchive::SaveActorsToBytes(const TArray<AActor*>& Actors, TArray<uint8>& OutBytes)
{
	// 씬 저장과 같이 Transient 액터(에디터 전용 등)는 복사하지 않는다.
	TArray<AActor*> SavedActors;
	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->HasAnyFlags(EObjectFlags::RF_Transient))
			SavedActors.Add(Actor);
	}
	if (SavedActors.IsEmpty())
		return false;

	OutBytes.Reset();
	FMemoryWriter Writer(OutBytes);
	{
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();
		int32 Version = SceneVersion;
		Root << SA_VALUE("Version", Version);

		int32 Num = SavedActors.Num();
		FStructuredArchive::FArray ActorArray = Root.EnterArray("Actors", Num);
		for (AActor* Actor : SavedActors)
		{
			ULevel::SaveActorRecord(ActorArray.EnterElement().EnterRecord(), Actor);
		}

		Ar.Close();
	}

	return !Writer.IsError();
}

bool FJsonArchive::SpawnActorsFromBytes(UWorld* World, const TArray<uint8>& Bytes, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (!World || Bytes.IsEmpty())
		return false;

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);
	if (Reader.IsError())
		return false;

	FStructuredArchive Ar(Formatter);
	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

	int32 Version = 0;
	Root << SA_VALUE("Version", Version);
	if (Version != SceneVersion)
		return false;

	int32 Num = 0;
	FStructuredArchive::FArray ActorArray = Root.EnterArray("Actors", Num);
	for (int32 i = 0; i < Num; ++i)
	{
		if (AActor* Actor = World->GetPersistentLevel()->SpawnActorFromRecord(ActorArray.EnterElement().EnterRecord()))
		{
			OutActors.Add(Actor);
		}
	}
	Ar.Close();

	return !Reader.IsError();
}

bool FJsonArchive::SaveWorld(UWorld* World, const FString& Path)
{
	TArray<uint8> Bytes;
	if (!SaveWorldToBytes(World, Bytes))
		return false;

	std::ofstream File(Path, std::ios::binary);
	if (!File.is_open())
		return false;
	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
	return !File.fail();
}

bool FJsonArchive::LoadWorld(UWorld* World, const FString& Path)
{
	if (!std::filesystem::exists(Path))
	{
		HTR_LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}

	std::ifstream File(Path, std::ios::binary);
	if (!File.is_open())
		return false;

	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(std::filesystem::file_size(Path)));
	File.read(reinterpret_cast<char*>(Bytes.GetData()), Bytes.Num());
	if (File.fail())
		return false;

	return LoadWorldFromBytes(World, Bytes);
}

bool FJsonArchive::SaveSequence(UMovieSceneSequence* Sequence, const FString& Path)
{
	TArray<uint8> Bytes;
	if (!SaveSequenceToBytes(Sequence, Bytes))
	{
		return false;
	}

	std::ofstream File(PathUtils::FromUtf8(Path), std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}

	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());

	return !File.fail();
}

bool FJsonArchive::SaveSequenceToBytes(UMovieSceneSequence* Sequence, TArray<uint8>& OutBytes)
{
	if (!Sequence)
	{
		return false;
	}

	FMemoryWriter Writer(OutBytes);
	{
		// 선언 순서 = 소멸 역순: Ar(Close) → Formatter(dump → Writer)
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

		int32 Version = SequenceVersion;
		Root << SA_VALUE("Version", Version);

		Sequence->Serialize(Root.EnterRecord("Sequence"));

		Ar.Close();
	}

	return !Writer.IsError();
}

bool FJsonArchive::LoadSequence(UMovieSceneSequence* Sequence, const FString& Path)
{
	const std::filesystem::path FilePath = PathUtils::FromUtf8(Path);
	if (!std::filesystem::exists(FilePath))
	{
		HTR_LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}

	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}

	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(std::filesystem::file_size(FilePath)));
	File.read(reinterpret_cast<char*>(Bytes.GetData()), Bytes.Num());
	if (File.fail())
	{
		return false;
	}

	return LoadSequenceFromBytes(Sequence, Bytes);
}

bool FJsonArchive::LoadSequenceFromBytes(UMovieSceneSequence* Sequence, const TArray<uint8>& Bytes)
{
	if (!Sequence)
	{
		return false;
	}

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);
	if (Reader.IsError())
	{
		HTR_LOG(Error, "Load: invalid sequence data");
		return false;
	}

	FStructuredArchive Ar(Formatter);
	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

	int32 Version = 0;
	Root << SA_VALUE("Version", Version);
	if (Version != SequenceVersion)
	{
		HTR_LOG(Error, "Load: sequence version {} is not supported (expected {})", Version, SequenceVersion);
		return false;
	}

	Sequence->Serialize(Root.EnterRecord("Sequence"));
	Ar.Close();

	return !Reader.IsError();
}

bool FJsonArchive::SaveBlendSpace(UBlendSpace1D* BlendSpace, const FString& Path)
{
	TArray<uint8> Bytes;
	if (!SaveBlendSpaceToBytes(BlendSpace, Bytes))
	{
		return false;
	}
	std::ofstream File(PathUtils::FromUtf8(Path), std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}
	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
	return !File.fail();
}

bool FJsonArchive::SaveBlendSpaceToBytes(UBlendSpace1D* BlendSpace, TArray<uint8>& OutBytes)
{
	if (!BlendSpace)
	{
		return false;
	}
	FMemoryWriter Writer(OutBytes);

	{
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();
		int32 Version = BlendSpaceVersion;
		Root << SA_VALUE("Version", Version);
		BlendSpace->Serialize(Root.EnterRecord("BlendSpace"));
		Ar.Close();
	}
	return !Writer.IsError();
}

bool FJsonArchive::LoadBlendSpace(UBlendSpace1D* BlendSpace, const FString& Path)
{
	const std::filesystem::path FilePath = PathUtils::FromUtf8(Path);
	if (!std::filesystem::exists(FilePath))
	{
		HTR_LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}
	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(std::filesystem::file_size(FilePath)));
	File.read(reinterpret_cast<char*>(Bytes.GetData()), Bytes.Num());
	if (File.fail())
	{
		return false;
	}

	return LoadBlendSpaceFromBytes(BlendSpace, Bytes);
}

bool FJsonArchive::LoadBlendSpaceFromBytes(UBlendSpace1D* BlendSpace, const TArray<uint8>& Bytes)
{
	if (!BlendSpace)
	{
		return false;
	}
	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);
	if (Reader.IsError())
	{
		HTR_LOG(Error, "Load: invalid BlendSpace data");
		return false;
	}

	FStructuredArchive Ar(Formatter);
	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();
	int32 Version = 0;
	Root << SA_VALUE("Version", Version);

	if (Version != BlendSpaceVersion)
	{
		HTR_LOG(Error, "Load: BlendSpace version {} is not supported (expected {})", Version, BlendSpaceVersion);
		return false;
	}

	BlendSpace->Serialize(Root.EnterRecord("BlendSpace"));

	Ar.Close();

	if (Reader.IsError() || !BlendSpace->IsBlendSpaceDataValid())
	{
		HTR_LOG(Error, "Load: invalid BlendSpace asset");
		return false;
	}

	return true;
}

bool FJsonArchive::SaveAnimationGraph(UAnimationGraph* Graph, const FString& Path)
{
	TArray<uint8> Bytes;

	if (!SaveAnimationGraphToBytes(Graph, Bytes))
	{
		return false;
	}

	std::ofstream File(PathUtils::FromUtf8(Path), std::ios::binary);

	if (!File.is_open())
	{
		return false;
	}

	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());

	return !File.fail();
}

bool FJsonArchive::SaveAnimationGraphToBytes(UAnimationGraph* Graph, TArray<uint8>& OutBytes)
{
	if (!Graph || !Graph->IsAnimationGraphDataValid())
	{
		return false;
	}

	FMemoryWriter Writer(OutBytes);

	{
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

		int32 Version = AnimationGraphVersion;

		Root << SA_VALUE("Version", Version);

		Graph->Serialize(Root.EnterRecord("AnimationGraph"));

		Ar.Close();
	}

	return !Writer.IsError();
}

bool FJsonArchive::LoadAnimationGraph(UAnimationGraph* Graph, const FString& Path)
{
	const std::filesystem::path FilePath = PathUtils::FromUtf8(Path);

	if (!Graph || !std::filesystem::exists(FilePath))
	{
		return false;
	}

	std::ifstream File(FilePath, std::ios::binary);

	if (!File.is_open())
	{
		return false;
	}

	TArray<uint8> Bytes;

	Bytes.SetNum(static_cast<int32>(std::filesystem::file_size(FilePath)));

	File.read(reinterpret_cast<char*>(Bytes.GetData()), Bytes.Num());

	if (File.fail())
	{
		return false;
	}

	return LoadAnimationGraphFromBytes(Graph, Bytes);
}

bool FJsonArchive::LoadAnimationGraphFromBytes(UAnimationGraph* Graph, const TArray<uint8>& Bytes)
{
	if (!Graph)
	{
		return false;
	}

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);

	if (Reader.IsError())
	{
		return false;
	}

	FStructuredArchive Ar(Formatter);

	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

	int32 Version = 0;

	Root << SA_VALUE("Version", Version);

	if (Version != AnimationGraphVersion)
	{
		return false;
	}

	Graph->Serialize(Root.EnterRecord("AnimationGraph"));

	Ar.Close();

	return !Reader.IsError() && Graph->IsAnimationGraphDataValid();
}

bool FJsonArchive::SaveAnimNotifies(UAnimSequenceBase* Sequence, const FString& Path)
{
	if (!Sequence || Path.empty())
	{
		return false;
	}

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);

	{
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

		int32 Version = AnimNotifyVersion;
		Root << SA_VALUE("Version", Version);

		Sequence->SerializeNotifies(Root.EnterRecord("AnimationNotifies"));

		Ar.Close();
	}

	if (Writer.IsError())
	{
		return false;
	}

	const std::filesystem::path FilePath = PathUtils::FromUtf8(Path);

	std::error_code Error;
	std::filesystem::create_directories(FilePath.parent_path(), Error);

	if (Error)
	{
		return false;
	}

	std::ofstream File(FilePath, std::ios::binary);

	if (!File.is_open())
	{
		return false;
	}

	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());

	return !File.fail();
}

bool FJsonArchive::LoadAnimNotifies(UAnimSequenceBase* Sequence, const FString& Path)
{
	if (!Sequence || Path.empty())
	{
		return false;
	}

	const std::filesystem::path FilePath = PathUtils::FromUtf8(Path);

	if (!std::filesystem::exists(FilePath))
	{
		return true;
	}

	std::ifstream File(FilePath, std::ios::binary);

	if (!File.is_open())
	{
		return false;
	}

	const uintmax_t FileSize = std::filesystem::file_size(FilePath);

	if (FileSize == 0 || FileSize > static_cast<uintmax_t>(std::numeric_limits<int32>::max()))
	{
		return false;
	}

	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(FileSize));

	File.read(reinterpret_cast<char*>(Bytes.GetData()), static_cast<std::streamsize>(Bytes.Num()));

	if (File.fail())
	{
		return false;
	}

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);

	if (Reader.IsError())
	{
		return false;
	}

	FStructuredArchive Ar(Formatter);
	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

	int32 Version = 0;
	Root << SA_VALUE("Version", Version);

	if (Version != AnimNotifyVersion)
	{
		HTR_LOG(Error, "Load: animation notify version {} is not supported", Version);
		return false;
	}

	Sequence->SerializeNotifies(Root.EnterRecord("AnimationNotifies"));

	Ar.Close();

	return !Reader.IsError();
}

FString FJsonArchive::GetAnimationNotifyPath(const FString& AnimationAssetPath)
{
	FString NotifyPath = AnimationAssetPath;

	size_t Position = 0;

	while ((Position = NotifyPath.find("::", Position)) != FString::npos)
	{
		NotifyPath.replace(Position, 2, "_");
		++Position;
	}

	return NotifyPath + ".animnotify";
}