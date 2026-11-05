#pragma once

#include "MovieSceneSequencePlayer.h"

class ULevelSequence;
class ULevel;

class ULevelSceneSequencePlayer : public UMovieSceneSequencePlayer
{
	DECLARE_CLASS(ULevelSceneSequencePlayer, UMovieSceneSequencePlayer)

public:
	void Initialize(ULevelSequence* InSequence, EMovieSceneCompletionMode CompletionMode, ULevel* InLevel);

	UWorld* GetPlaybackWorld() const override;

private:
	UWorld* World;
};
