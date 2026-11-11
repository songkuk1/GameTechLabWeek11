#pragma once

#include "MovieSceneSequencePlayer.h"
#include "LevelSceneSequencePlayer.generated.h"

class ULevelSequence;
class ULevel;

UCLASS()
class ULevelSceneSequencePlayer : public UMovieSceneSequencePlayer
{
	GENERATED_BODY()

public:
	void Initialize(ULevelSequence* InSequence, EMovieSceneCompletionMode CompletionMode, ULevel* InLevel);

	UWorld* GetPlaybackWorld() const override;

private:
	UWorld* World;
};
