#pragma once

class UWorld;
class AActor;
class UMovieSceneSequence;

class FJsonArchive
{
public:
	static bool SaveWorld(UWorld* World, const FString& Path);
	static bool LoadWorld(UWorld* World, const FString& Path);
	static bool LoadWorldFromBytes(UWorld* World, const TArray<uint8>& Bytes);
	static bool SaveWorldToBytes(UWorld* World, TArray<uint8>& OutBytes);

	// 액터 목록을 씬 파일과 같은 형식으로 직렬화한다 (에디터 복사). 저장할 액터가 없으면 false다.
	static bool SaveActorsToBytes(const TArray<AActor*>& Actors, TArray<uint8>& OutBytes);
	// SaveActorsToBytes 결과로 World에 새 액터를 만든다 (에디터 붙여넣기). 기존 액터는 건드리지 않는다.
	static bool SpawnActorsFromBytes(UWorld* World, const TArray<uint8>& Bytes, TArray<AActor*>& OutActors);

	static bool SaveSequence(UMovieSceneSequence* Sequence, const FString& Path);
	static bool SaveSequenceToBytes(UMovieSceneSequence* Sequence, TArray<uint8>& OutBytes);

	static bool LoadSequence(UMovieSceneSequence* Sequence, const FString& Path);
	static bool LoadSequenceFromBytes(UMovieSceneSequence* Sequence, const TArray<uint8>& Bytes);
};
