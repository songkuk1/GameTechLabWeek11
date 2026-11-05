#include "EnginePCH.h"
#include "LevelSequence.h"
#include "MovieScene.h"

void ULevelSequence::Initialize(UMovieScene* InScene)
{
	MovieScene = InScene;
}

UMovieScene* ULevelSequence::GetMovieScene() const
{
	return MovieScene;
}

void ULevelSequence::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	FArchive& Archive = Record.GetUnderlyingArchive();
	if (Archive.IsLoading())
	{
		MovieScene = NewObject<UMovieScene>(this);
	}

	auto MovieSceneRecord = Record.EnterRecord("MovieScene");
	MovieScene->Serialize(MovieSceneRecord);
}
